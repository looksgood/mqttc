AUTOMAKE_OPTIONS = subdir-objects

CFLAGS = -g -Wall

SMARTA_FLAGS = -I src/

noinst_PROGRAMS = mqttc
mqttc_SOURCES = src/ae.c \
    src/anet.c src/sds.c \
	src/list.c src/logger.c \
	src/hash.c src/zmalloc.c \
	src/mqtt.c src/packet.c \
	src/client.c 

mqttc_CFLAGS = $(SMARTA_FLAGS)
mqttc_LDADD = $(SMARTA_LIBS) 
 
