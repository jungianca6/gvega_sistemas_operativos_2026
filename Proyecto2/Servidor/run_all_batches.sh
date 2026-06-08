#!/bin/bash

# Setup MPI environment - must be done before any command
source /opt/intel/oneapi/setvars.sh --force 2>/dev/null || true

# Get list of images
mapfile -t IMAGES < <(ls ../images/*.jpg 2>/dev/null | sort)
IMG_COUNT=${#IMAGES[@]}

if [ "$IMG_COUNT" -lt 3 ]; then
    echo "Error: Se necesitan al menos 3 imágenes en ../images"
    exit 1
fi

echo "Encontradas $IMG_COUNT imágenes"

# Find mpirun command
MPI_CMD=$(which mpirun 2>/dev/null || which mpiexec 2>/dev/null || true)
if [ -z "$MPI_CMD" ]; then
    echo "Error: mpirun/mpiexec no se encuentra en PATH."
    exit 1
fi

echo "Using: $MPI_CMD"

# Get MPI library paths from mpicc and set them for export
MPI_LD_PATH=$(mpicc -show 2>/dev/null | tr " " "\n" | grep "^-L" | sed "s/^-L//" | paste -sd ":" -)
if [ -z "$MPI_LD_PATH" ]; then
    MPI_LD_PATH=$(mpicc --showme:link 2>/dev/null | tr " " "\n" | grep "^-L" | sed "s/^-L//" | paste -sd ":" - || true)
fi

if [ -n "$MPI_LD_PATH" ]; then
    export LD_LIBRARY_PATH="${MPI_LD_PATH}:${LD_LIBRARY_PATH}"
fi

# Export MPI environment for mpirun to propagate
export I_MPI_HYDRA_BOOTSTRAP=ssh
export I_MPI_ROOT=/opt/intel/oneapi/mpi/2021.17

echo "LD_LIBRARY_PATH: $LD_LIBRARY_PATH"

# Process images in batches of 3
BATCH_NUM=1
for ((i=0; i<IMG_COUNT; i+=3)); do
    BATCH=("${IMAGES[@]:i:3}")
    BATCH_SIZE=${#BATCH[@]}
    
    if [ "$BATCH_SIZE" -lt 3 ]; then
        echo "Lote incompleto con $BATCH_SIZE imágenes omitido."
        continue
    fi
    
    echo "=== Lote $BATCH_NUM: ${BATCH[*]} ==="
    # Pass LD_LIBRARY_PATH explicitly to all MPI processes
    "$MPI_CMD" -genv LD_LIBRARY_PATH "$LD_LIBRARY_PATH" -np 4 ./Servidor "${BATCH[@]}"
    BATCH_NUM=$((BATCH_NUM+1))
done

echo "Procesamiento completado."
