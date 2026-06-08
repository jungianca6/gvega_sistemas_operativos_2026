#!/bin/bash

# Get list of images
mapfile -t IMAGES < <(ls ../images/*.jpg 2>/dev/null | sort)
IMG_COUNT=${#IMAGES[@]}

if [ "$IMG_COUNT" -lt 3 ]; then
    echo "Error: Se necesitan al menos 3 imágenes en ../images"
    exit 1
fi

echo "Encontradas $IMG_COUNT imágenes"

MPI_CMD=$(which mpirun 2>/dev/null || which mpiexec 2>/dev/null || true)
if [ -z "$MPI_CMD" ]; then
    echo "Error: mpirun/mpiexec no se encuentra en PATH."
    exit 1
fi

echo "Using: $MPI_CMD"

MPI_LD_PATH=$(mpicc -show 2>/dev/null | tr " " "\n" | grep "^-L" | sed "s/^-L//" | paste -sd ":" -)
if [ -z "$MPI_LD_PATH" ]; then
    MPI_LD_PATH=$(mpicc --showme:link 2>/dev/null | tr " " "\n" | grep "^-L" | sed "s/^-L//" | paste -sd ":" - || true)
fi

if [ -n "$MPI_LD_PATH" ]; then
    export LD_LIBRARY_PATH="${MPI_LD_PATH}:${LD_LIBRARY_PATH}"
fi

echo "LD_LIBRARY_PATH: $LD_LIBRARY_PATH"

BATCH_NUM=1
for ((i=0; i<IMG_COUNT; i+=3)); do
    BATCH=("${IMAGES[@]:i:3}")
    BATCH_SIZE=${#BATCH[@]}

    if [ "$BATCH_SIZE" -lt 3 ]; then
        echo "Lote incompleto con $BATCH_SIZE imágenes omitido."
        continue
    fi

    echo "=== Lote $BATCH_NUM: ${BATCH[*]} ==="
    "$MPI_CMD" -x LD_LIBRARY_PATH -np 4 ./Servidor "${BATCH[@]}"
    BATCH_NUM=$((BATCH_NUM+1))
done

echo "Procesamiento completado."
