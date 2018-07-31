OBJ=zepass/pass.o \
	zepass/decoder.o \
	usrp/usrp.o \
	main.o

OFLAGS=-O3
DEFINES=
CXXFLAGS=-std=c++14 -g -I. -Wall -Wextra $(DEFINES) $(OFLAGS)

LIBS=-lfftw3 -lm -lboost_program_options -lboost_system -luhd
LDFLAGS=$(LIBS)

inc=$(OBJ:%.o=%.d)

TARGET=zepassd

$(TARGET): $(OBJ)
	$(CXX) -o $(TARGET) $(OBJ) $(LDFLAGS)

-include $(inc)

.cpp.o:
	$(CXX) $(CXXFLAGS) -MMD -MP -o $@ -c $<

clean:
	$(RM) $(TARGET)
	$(RM) $(OBJ)
	$(RM) $(inc)

.PHONY: clean
