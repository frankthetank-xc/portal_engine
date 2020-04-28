.PHONY: build clean

export
build:
	$(MAKE) -C ./src 

clean:
	$(MAKE) -C ./src clean
