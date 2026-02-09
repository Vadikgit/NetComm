BUILD := build
OBJECTS := $(BUILD)/objects

ALL_LDFLAGS := $(LDFLAGS) $(EXTRA_LDFLAGS)

clean:
	@rm -rf $(BUILD)/*
	@rm -rf $(BUILD)

# Directory
$(BUILD):
	$(Q)mkdir -p $@

$(OBJECTS): | $(BUILD)
	$(Q)mkdir -p $@

$(OBJECTS)/server_main.o: server_main.cpp | $(OBJECTS)
	@$(CXX) $(CXXFLAGS) -g -c server_main.cpp -o $@

$(OBJECTS)/client_main.o: client_main.cpp | $(OBJECTS)
	@$(CXX) $(CXXFLAGS) -g -c client_main.cpp -o $@

$(BUILD)/server_app: $(OBJECTS)/server_main.o $(BUILD)/libnetcomm.so | $(OBJECTS)
	@$(CXX) $(CXXFLAGS) $(OBJECTS)/server_main.o $(ALL_LDFLAGS) -g -lelf -lz -L$(BUILD) -lnetcomm -Wl,-rpath,. -o $@ 
	
$(BUILD)/client_app: $(OBJECTS)/client_main.o $(BUILD)/libnetcomm.so | $(OBJECTS)
	@$(CXX) $(CXXFLAGS) $(OBJECTS)/client_main.o $(ALL_LDFLAGS) -g -lelf -lz -L$(BUILD) -lnetcomm -Wl,-rpath,. -o $@ 

complete: $(BUILD)/client_app $(BUILD)/server_app
	@rm -rf $(OBJECTS)
	cp common.h client.h server.h $(BUILD)

$(BUILD)/libnetcomm.so: $(OBJECTS)/client.o $(OBJECTS)/server.o
	@$(CXX) $(CXXFLAGS) -shared -g $^ -o $@

$(OBJECTS)/client.o: client.cpp | $(OBJECTS)
	@$(CXX) $(CXXFLAGS) -g -c -fPIC client.cpp -o $@

$(OBJECTS)/server.o: server.cpp | $(OBJECTS)
	@$(CXX) $(CXXFLAGS) -g -c -fPIC server.cpp -o $@

.DELETE_ON_ERROR:

.SECONDARY:
