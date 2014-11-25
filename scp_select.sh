source var.sh
set -x

scp $@ $LOGIN@130.245.156.19:~/$PROJ_FOLDER/.
