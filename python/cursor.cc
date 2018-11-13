#include <Python.h>
#include <dballe/core/record.h>
#include "cursor.h"
#include "record.h"
#include "db.h"
#include "common.h"
#include <algorithm>
#include "impl-utils.h"

#if PY_MAJOR_VERSION <= 2
    #define PyLong_FromLong PyInt_FromLong
#endif

using namespace std;
using namespace dballe;
using namespace dballe::python;
using namespace wreport;

extern "C" {
PyTypeObject* dpy_Cursor_Type = nullptr;
}

namespace {

void ensure_valid_cursor(dpy_Cursor* self)
{
    if (self->cur == nullptr)
    {
        PyErr_SetString(PyExc_RuntimeError, "cannot access a cursor after the with block where it was used");
        throw PythonException();
    }
}

struct remaining : Getter<dpy_Cursor>
{
    constexpr static const char* name = "remaining";
    constexpr static const char* doc = "number of results still to be returned";
    static PyObject* get(Impl* self, void* closure)
    {
        try {
            ensure_valid_cursor(self);
            return PyLong_FromLong(self->cur->remaining());
        } DBALLE_CATCH_RETURN_PYO
    }
};

struct query_attrs : MethKwargs<dpy_Cursor>
{
    constexpr static const char* name = "query_attrs";
    constexpr static const char* signature = "attrs: Iterable[str]";
    constexpr static const char* returns = "Dict[str, Any]";
    constexpr static const char* doc = "Query attributes for the current variable (deprecated)";
    static PyObject* run(Impl* self, PyObject* args, PyObject* kw)
    {
        try {
            ensure_valid_cursor(self);

            if (PyErr_WarnEx(PyExc_DeprecationWarning, "please use Cursor.attr_query, DB.attr_query_station or DB.attr_query_data instead of Cursor.query_attrs", 1))
                return nullptr;

            static const char* kwlist[] = { "attrs", NULL };
            PyObject* attrs = 0;
            if (!PyArg_ParseTupleAndKeywords(args, kw, "|O", const_cast<char**>(kwlist), &attrs))
                return nullptr;

            // Read the attribute list, if provided
            db::AttrList codes = db_read_attrlist(attrs);

            pyo_unique_ptr res(throw_ifnull(PyDict_New()));

            if (auto c = dynamic_cast<const db::CursorStationData*>(self->cur))
            {
                c->get_transaction()->attr_query_station(c->attr_reference_id(), [&](unique_ptr<Var>&& var) {
                    if (!codes.empty() && find(codes.begin(), codes.end(), var->code()) == codes.end())
                        return;
                    set_var(res, *var);
                });
            }
            else if (auto c = dynamic_cast<const db::CursorData*>(self->cur))
            {
                c->get_transaction()->attr_query_data(c->attr_reference_id(), [&](unique_ptr<Var>&& var) {
                    if (!codes.empty() && find(codes.begin(), codes.end(), var->code()) == codes.end())
                        return;
                    set_var(res, *var);
                });
            }
            else
            {
                PyErr_SetString(PyExc_ValueError, "the cursor does ont come from DB.query_station_data or DB.query_data");
                return nullptr;
            }
            return (PyObject*)res.release();
        } DBALLE_CATCH_RETURN_PYO
    }
};

struct attr_query : MethNoargs<dpy_Cursor>
{
    constexpr static const char* name = "attr_query";
    constexpr static const char* returns = "Dict[str, Any]";
    constexpr static const char* summary = "Query attributes for the current variable";
    static PyObject* run(Impl* self)
    {
        try {
            ensure_valid_cursor(self);
            pyo_unique_ptr res(throw_ifnull(PyDict_New()));
            if (auto c = dynamic_cast<const db::CursorStationData*>(self->cur))
            {
                c->get_transaction()->attr_query_station(c->attr_reference_id(), [&](unique_ptr<Var>&& var) {
                    set_var(res, *var);
                });
            }
            else if (auto c = dynamic_cast<const db::CursorData*>(self->cur))
            {
                c->get_transaction()->attr_query_data(c->attr_reference_id(), [&](unique_ptr<Var>&& var) {
                    set_var(res, *var);
                });
            }
            else
            {
                PyErr_SetString(PyExc_ValueError, "the cursor does ont come from DB.query_station_data or DB.query_data");
                return nullptr;
            }
            return (PyObject*)res.release();
        } DBALLE_CATCH_RETURN_PYO
    }
};

typedef MethGenericEnter<dpy_Cursor> __enter__;

struct __exit__ : MethVarargs<dpy_Cursor>
{
    constexpr static const char* name = "__exit__";
    constexpr static const char* doc = "Context manager __exit__";
    static PyObject* run(Impl* self, PyObject* args)
    {
        PyObject* exc_type;
        PyObject* exc_val;
        PyObject* exc_tb;
        if (!PyArg_ParseTuple(args, "OOO", &exc_type, &exc_val, &exc_tb))
            return nullptr;

        try {
            ReleaseGIL gil;
            delete self->cur;
            self->cur = nullptr;
        } DBALLE_CATCH_RETURN_PYO
        Py_RETURN_NONE;
    }
};

struct Definition : public Binding<Definition, dpy_Cursor>
{
    constexpr static const char* name = "Cursor";
    constexpr static const char* qual_name = "dballe.Cursor";
    constexpr static const char* summary = "cursor iterating dballe.DB query results";
    constexpr static const char* doc = R"(
a Cursor is the result of database queries. It is generally not used explicitly
and just iterated.
)";

    GetSetters<remaining> getsetters;
    Methods<__enter__, __exit__, query_attrs, attr_query> methods;

    static void _dealloc(Impl* self)
    {
        delete self->cur;
        Py_TYPE(self)->tp_free(self);
    }

    static PyObject* _iter(Impl* self)
    {
        try {
            ensure_valid_cursor(self);
            Py_INCREF(self);
            return (PyObject*)self;
        } DBALLE_CATCH_RETURN_PYO
    }

    static PyObject* _iternext(Impl* self)
    {
        try {
            ensure_valid_cursor(self);
            if (self->cur->next())
            {
                Py_INCREF(self);
                return (PyObject*)self;
            } else {
                PyErr_SetNone(PyExc_StopIteration);
                return nullptr;
            }
        } DBALLE_CATCH_RETURN_PYO
    }
};

Definition* definition = nullptr;

}

namespace dballe {
namespace python {

dpy_Cursor* cursor_create(std::unique_ptr<Cursor> cur)
{
    py_unique_ptr<dpy_Cursor> result(throw_ifnull(PyObject_New(dpy_Cursor, dpy_Cursor_Type)));
    result->cur = cur.release();
    return result.release();
}

void register_cursor(PyObject* m)
{
    common_init();

    definition = new Definition;
    dpy_Cursor_Type = definition->activate(m);
}

}
}
