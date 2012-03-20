# - read version from file and set version variables
#
# sets the following vars
#  ${name}_VERSION
#  ${name}_VERSION_NUMBER
#  ${name}_VERSION_MAJOR
#  ${name}_VERSION_MINOR
#  ${name}_VERSION_PATCH
#  ${name}_VERSION_BUILD

macro(read_version)

  set(_name ${ARGV0})
  if(NOT _name)
    set(_name ${PROJECT_NAME})
  endif()

  set(_file ${ARGV1})
  if(NOT _file)
    set(_file "${CMAKE_CURRENT_SOURCE_DIR}/config/VERSION")
  endif()

  set(_var_name ${_name}_VERSION)

  file(READ ${_file} ${_var_name})
  string(REPLACE "\n" "" ${_var_name} ${${_var_name}})
  string(REPLACE "." ";" ver_list ${${_var_name}})
  list(LENGTH ver_list ver_len)

  list(GET ver_list 0 ${_var_name}_MAJOR)
  list(GET ver_list 1 ${_var_name}_MINOR)
  list(GET ver_list 2 ${_var_name}_PATCH)

  # sanity check version string
  if(ver_len EQUAL 4)
    list(GET ver_list 3 ${_var_name}_BUILD)
    # drop build octet from string if 0
    if(${_var_name}_BUILD EQUAL 0)
      set(${_var_name}
        "${${_var_name}_MAJOR}.${${_var_name}_MINOR}.${${_var_name}_PATCH}")
    endif()

  elseif(ver_len EQUAL 3)
    set(${_var_name}_BUILD 0)

  else()
    message(FATAL_ERROR "no version defined")
  endif()

  # export version vars
  MATH(EXPR ${_var_name}_NUMBER
    "(${${_var_name}_MAJOR} <<24) | (${${_var_name}_MINOR} <<16) | (${${_var_name}_PATCH} <<8) | ${${_var_name}_BUILD}" )

endmacro(read_version)

