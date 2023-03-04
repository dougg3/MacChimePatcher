OBJ = inject_chime.o ../util/adler32.o ../util/ima.o ../util/md5.o

%.o: %.cpp
	$(CXX) -c -o $@ $<

inject_chime: $(OBJ)
	$(CXX) -o $@ $^

.PHONY: clean

clean:
	rm -f $(OBJ) inject_chime
