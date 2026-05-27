cmake_minimum_required(VERSION 4.2)
project(Servidor C)

set(CMAKE_C_STANDARD 17)

find_package(MPI REQUIRED)
find_package(OpenMP REQUIRED)

add_executable(Servidor main.c)

target_link_libraries(Servidor
        MPI::MPI_C
        OpenMP::OpenMP_C
)