obj-m += content.o
content-objs := module.o ctable.o csock.o trans.o send.o send_handler.o recv.o

all:
	make -C /lib/modules/$(shell uname -r)/build M=$(PWD) modules

clean:
	make -C /lib/modules/$(shell uname -r)/build M=$(PWD) clean
	rm -f *~
