CC=arm-linux-gnueabihf-
DIR=./lib/
target:
	$(CC)gcc -O3 -o slave $(DIR)udp.c $(DIR)uart.c $(DIR)crc.c $(DIR)log.c AM335X_2.c global.c aws.c rkt.c bd.c modbus_polling.c -I$(DIR) -lpthread -lm  
clean:
	@rm -vf slave

