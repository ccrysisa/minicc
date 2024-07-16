CFLAGS=-m32 -Wall -Werror -Wextra -g

minicc: minicc.c 
	$(CC) $(CFLAGS) -o $@ $<

clean:
	@rm -rf minicc *,o *.out
