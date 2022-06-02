export IN=mario.png
export OUT=mario.bin
export X=5
export Y=0
export RATE=48000
export PPM=1500
##########
echo "Playing..."
./tool $IN -o $OUT -v -x $X -y $Y -p $PPM -r $RATE && aplay -f s16_le -r $RATE $OUT
echo "Done."
