#ifndef DBALLE_PYTHON_EXPORTER_H
#define DBALLE_PYTHON_EXPORTER_H

#include <Python.h>
#include <dballe/exporter.h>
#include <memory>

extern "C" {

typedef struct {
    PyObject_HEAD
    dballe::Exporter* exporter;
} dpy_Exporter;

extern PyTypeObject* dpy_Exporter_Type;

#define dpy_Exporter_Check(ob) \
    (Py_TYPE(ob) == dpy_Exporter_Type || \
     PyType_IsSubtype(Py_TYPE(ob), dpy_Exporter_Type))

}


namespace dballe {
namespace python {

/**
 * Create a new dpy_Exporter
 */
dpy_Exporter* exporter_create(Encoding type, const ExporterOptions& opts=ExporterOptions());

void register_exporter(PyObject* m);

}
}

#endif



