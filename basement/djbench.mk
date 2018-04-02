#LIBJPEG=../../jpeg-9.armv6/.libs
#LIBJPEG=../../jpeg-9.armv7/.libs
#LIBJPEG=../../jpeg-8/.libs
LIBJPEG=../../libjpeg-turbo/build

djbench: djbench.o ../ArrayU8.o ../Mem.o ../Cfg.o ../Images.o ../locks.o ../String.o ../jpeghufftables.o ../jpeg_misc.o ../dpf.o ../cti_utils.o $(LIBJPEG)/libjpeg.a
	gcc -o djbench $^ -lpthread -o djbench

djbench.o: djbench.c
	gcc -c djbench.c

../%.o: ../%.c
	(cd ..; make $(subst ../,,$@))
