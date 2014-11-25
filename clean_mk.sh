source var.sh
set -x

ssh $LOGIN@130.245.156.19 "cd ~/$PROJ_FOLDER/;make clean"
