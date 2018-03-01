CUR_DIR = /home/roc/unix/cortana

IP_DIR = ${CUR_DIR}/ip
NET_DIR = ${CUR_DIR}/net
UTIL_DIR = ${CUR_DIR}/util
SERVER_DIR = ${CUR_DIR}/example/echo

INC_DIR = -I${IP_DIR} \
		  -I${NET_DIR}\
		  -I${BASE_DIR}\
		  -I${UTIL_DIR}\
		  -I${SERVER_DIR}

SRC = $(wildcard ${IP_DIR}/*.cc) \
	  $(wildcard ${NET_DIR}/*.cc) \
	  $(wildcard ${BASE_DIR}/*.cc) \
	  $(wildcard ${UTIL_DIR}/*.cc) \
	  $(wildcard ${SERVER_DIR}/*.cc)

OBJ = $(patsubst %.cc, %.o, ${SRC})

TARGET = server
CXX = g++
CFLAGS = -g -Wall -Werror ${INC_DIR} -std=c++17  

${TARGET}: ${OBJ}
	${CXX} ${OBJ} -o $@ -lpthread
	@echo "Compile done."

#${OBJ}:${SRC}
#   $(CC) ${CCFLAGS} -c $? 

$(OBJ):%.o:%.cc
	@echo "Compiling $< ==> $@"
	${CXX} ${CFLAGS} -c $< -o $@

clean:
	@rm -f ${OBJ}
	@echo "Clean object files done."
	@rm -f *~
	@echo "Clean tempreator files done."
	@rm -f ${TARGET}
	@echo "Clean target files done."
	@echo "Clean done."
