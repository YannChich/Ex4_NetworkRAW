all: ping watchdog new_ping
ping: ping.c
	gcc -Wall -g ping.c -o parta
	sudo setcap cap_net_raw+ep parta
watchdog: watchdog.c
	gcc -Wall -g watchdog.c -o watchdog
new_ping: new_ping.c
	gcc -Wall -g new_ping.c -o partb
	sudo setcap cap_net_raw+ep partb

clean:
	rm -f *.o parta watchdog partb