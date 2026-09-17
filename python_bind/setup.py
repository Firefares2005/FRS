from setuptools import setup, Extension
import pybind11

ext_modules = [
    Extension(
        "frs_python",
        sources=[
            "pyfrs.cpp",
            "frs.cpp",
            "src/entropy.cpp",
            "src/image_io.cpp",
            "src/encoder.cpp",
            "src/decoder.cpp",
        ],
        include_dirs=[pybind11.get_include(), ".", "src"],
        language="c++",
    )
]

setup(
    name="frs-codec",
    version="1.1.0",
    author="Firefares2005",
    author_email="firefares2005@example.com",
    description="FRS image compression library - Python bindings",
    long_description=open("README.md", encoding="utf-8").read(),
    long_description_content_type="text/markdown",
    url="https://github.com/Firefares2005/FRS",
    license="MIT",
    ext_modules=ext_modules,
    python_requires=">=3.8",
    install_requires=["pybind11>=2.10"],
    classifiers=[
        "Programming Language :: C++",
        "Programming Language :: Python :: 3",
        "Operating System :: OS Independent",
        "Topic :: Multimedia :: Graphics :: Graphics Conversion",
    ],
)
