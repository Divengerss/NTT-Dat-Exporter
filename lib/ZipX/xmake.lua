add_rules("mode.debug", "mode.release")

set_languages("c++20")
set_policy("build.warning", true)
set_warnings("all", "error")
add_cxxflags("-Wall", "-O2")

add_requires("zlib")

target("ZipX")
    set_kind("shared")
    set_basename("ZipXlib")
    add_includedirs("include", "$(projectdir)/include", {public = true})
    add_files("src/*.cpp")
    add_packages("zlib")
    add_rules("utils.symbols.export_all", {export_classes = true})