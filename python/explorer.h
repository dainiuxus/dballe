#ifndef DBALLE_PYTHON_EXPLORER_H
#define DBALLE_PYTHON_EXPLORER_H

#include <Python.h>
#include <dballe/db/explorer.h>

extern "C" {

typedef struct {
    PyObject_HEAD
    dballe::db::Explorer* explorer;
} dpy_Explorer;

PyAPI_DATA(PyTypeObject) dpy_Explorer_Type;

#define dpy_Explorer_Check(ob) \
    (Py_TYPE(ob) == &dpy_Explorer_Type || \
     PyType_IsSubtype(Py_TYPE(ob), &dpy_Explorer_Type))

}

namespace dballe {
namespace python {

/**
 * Create a new dpy_Explorer for an existing DB
 */
dpy_Explorer* explorer_create();

/**
 * Create a new dpy_Explorer, taking over memory management
 */
dpy_Explorer* explorer_create(std::unique_ptr<dballe::db::Explorer> explorer);

void register_explorer(PyObject* m);

}
}

#endif