#include <pybind11/pybind11.h>

namespace py = pybind11;

PYBIND11_MODULE(lupine_engine_test, m) {
    m.doc() = "Minimal test module";
    m.attr("__version__") = "0.1.0";

    m.def("test_function", []() {
        return "Hello from minimal module!";
    });
}

