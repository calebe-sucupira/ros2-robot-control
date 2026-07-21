#!/usr/bin/env bash

set -euo pipefail

PROJECT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
REPOSITORY_DIR="$(cd "${PROJECT_DIR}/../.." && pwd)"
COMPONENT_DIR="${PROJECT_DIR}/components/micro_ros_espidf_component"
MICROROS_MAKEFILE="${COMPONENT_DIR}/libmicroros.mk"

git -C "${REPOSITORY_DIR}" submodule update --init --recursive

if [[ ! -f "${MICROROS_MAKEFILE}" ]]; then
    echo "Erro: componente micro-ROS não encontrado." >&2
    exit 1
fi

BACKUP_FILE="$(mktemp)"
cp "${MICROROS_MAKEFILE}" "${BACKUP_FILE}"

restore_component() {
    cp "${BACKUP_FILE}" "${MICROROS_MAKEFILE}"
    rm -f "${BACKUP_FILE}"
}

trap restore_component EXIT

if grep -q -- "-DCMAKE_CXX_COMPILER=gcc" "${MICROROS_MAKEFILE}"; then
    sed -i \
        's/-DCMAKE_CXX_COMPILER=gcc/-DCMAKE_CXX_COMPILER=g++/' \
        "${MICROROS_MAKEFILE}"
elif ! grep -q -- "-DCMAKE_CXX_COMPILER=g++" "${MICROROS_MAKEFILE}"; then
    echo "Erro: configuração do compilador C++ não reconhecida." >&2
    exit 1
fi

cd "${PROJECT_DIR}"
idf.py build
