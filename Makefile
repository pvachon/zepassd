OBJ=zepass/pass.o \
	zepass/decoder.o \
	usrp/usrp.o \
	main.o

OFLAGS=-O3
DEFINES=

CPPFLAGS=$(DEFINES)
CXXFLAGS=-std=c++14 -g -I. -Wall -Wextra -MMD -MP $(OFLAGS)

LIBS=-lfftw3 -lm -lboost_program_options -lboost_system -luhd

LDFLAGS=$(LIBS)

inc=$(OBJ:%.o=%.d)

TARGET=zepassd

$(TARGET): $(OBJ)
	$(CXX) -o $(TARGET) $(OBJ) $(LDFLAGS)

-include $(inc)

clean:
	$(RM) $(TARGET)
	$(RM) $(OBJ)
	$(RM) $(inc)

.PHONY: clean
