all: rftp

rftp: clean
	g++ -g -Wno-deprecated crc32.h crc32.c cache.h cache.cpp rftp_protocol.h rftp_protocol.cpp rftp_main.cpp -o rftp -lpthread

clean: 
	rm -rf *~ rftp
