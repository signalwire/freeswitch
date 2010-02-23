for file in *.m4a; do name=`echo $file | sed "s/\ /\_/g" | sed "s/.m4a//"`; \
    echo "Convert m4a $file to $name.wav"; faad -o $name.wav "$file" > /dev/null 2>&1; \
    for rate in 8000 16000 32000 48000; do mkdir -p $rate; \
    	echo "Creating $rate kHz -> $rate/$name.wav"; \
    	sox $name.wav -c 1 -r $rate $rate/$name.wav vol 0.06; \
    done; \
    rm -f *.wav; \
done;
