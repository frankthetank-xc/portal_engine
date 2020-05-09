.PHONY: build clean

export
build:
	-mkdir ./obj
	$(MAKE) -C ./src 

clean:
	$(MAKE) -C ./src clean
