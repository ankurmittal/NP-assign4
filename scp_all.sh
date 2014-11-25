source var.sh
set -x

scp *.c *.h Makefile README *.mk $LOGIN@130.245.156.19:~/$PROJ_FOLDER/.
