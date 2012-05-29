from time import sleep

def rimz_func(io):
	#sleep(0.15)
	input_data = io.read()
	io.write(input_data + "1")
	io.write(input_data + "2")
	#raise RuntimeError("huita happened at rimz app!")
