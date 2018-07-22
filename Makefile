OBJ=zepass/pass.o \
	zepass/decoder.o \
	usrp/usrp.o \
	main.o

OFLAGS=-O3
DEFINES=
CXXFLAGS=-std=c++14 -g -I. -Wall -Wextra $(DEFINES) $(OFLAGS)

LIBS=-lfftw3 -lm -lboost_program_options -lboost_system -luhd
LDFLAGS=$(LIBS)

TARGET=zepassd

$(TARGET): $(OBJ)
	$(CXX) -o $(TARGET) $(OBJ) $(LDFLAGS)

.cpp.o:
	$(CXX) $(CXXFLAGS) -o $@ -c $<

clean:
	$(RM) $(TARGET)
	$(RM) $(OBJ)

.PHONY: clean
