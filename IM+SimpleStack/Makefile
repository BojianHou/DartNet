all: son/son sip/sip client/IM_client server/IM_server  

common/pkt.o: common/pkt.c common/pkt.h common/constants.h
	gcc -Wall -pedantic -std=c99 -g -c common/pkt.c -o common/pkt.o
topology/topology.o: topology/topology.c 
	gcc -Wall  -g -c topology/topology.c -o topology/topology.o
son/neighbortable.o: son/neighbortable.c
	gcc -Wall -pedantic -std=c99 -g -c son/neighbortable.c -o son/neighbortable.o
son/son: topology/topology.o common/pkt.o son/neighbortable.o son/son.c 
	gcc -Wall -pedantic -std=c99 -g -pthread son/son.c topology/topology.o common/pkt.o son/neighbortable.o -o son/son
sip/nbrcosttable.o: sip/nbrcosttable.c
	gcc -Wall -pedantic -std=c99 -g -c sip/nbrcosttable.c -o sip/nbrcosttable.o
sip/dvtable.o: sip/dvtable.c
	gcc -Wall -pedantic -std=c99 -g -c sip/dvtable.c -o sip/dvtable.o
sip/routingtable.o: sip/routingtable.c
	gcc -Wall -pedantic -std=c99 -g -c sip/routingtable.c -o sip/routingtable.o
sip/sip: common/pkt.o common/seg.o topology/topology.o sip/nbrcosttable.o sip/dvtable.o sip/routingtable.o sip/sip.c 
	gcc -Wall -pedantic -std=c99 -g -pthread sip/nbrcosttable.o sip/dvtable.o sip/routingtable.o common/pkt.o common/seg.o topology/topology.o sip/sip.c -o sip/sip 
client/IM_client: client/IM_client.c common/seg.o stcp/stcp_client.o stcp/stcp_server.o topology/topology.o stcp/seghandler.o common/head.h
	gcc -Wall -pedantic -std=c99 -g -pthread client/IM_client.c common/seg.o stcp/stcp_client.o stcp/stcp_server.o topology/topology.o stcp/seghandler.o -o client/IM_client 
server/IM_server: server/IM_server.c common/seg.o stcp/stcp_client.o stcp/stcp_server.o topology/topology.o stcp/seghandler.o common/head.h
	gcc -Wall -pedantic -std=c99 -g -pthread server/IM_server.c common/seg.o stcp/stcp_client.o stcp/stcp_server.o topology/topology.o stcp/seghandler.o -o server/IM_server
common/seg.o: common/seg.c common/seg.h
	gcc -Wall -pedantic -std=c99 -g -c common/seg.c -o common/seg.o
stcp/seghandler.o: stcp/seghandler.c stcp/seghandler.h 
	gcc -Wall -g -c stcp/seghandler.c -o stcp/seghandler.o
stcp/stcp_client.o: stcp/stcp_client.c stcp/stcp_client.h 
	gcc -Wall -g -c stcp/stcp_client.c -o stcp/stcp_client.o
stcp/stcp_server.o: stcp/stcp_server.c stcp/stcp_server.h
	gcc -Wall -g -c stcp/stcp_server.c -o stcp/stcp_server.o

clean:
	rm -rf common/*.o
	rm -rf topology/*.o
	rm -rf son/*.o
	rm -rf son/son
	rm -rf sip/*.o
	rm -rf sip/sip 
	rm -rf stcp/*.o
	rm -rf client/*.o
	rm -rf server/*.o
	rm -rf client/IM_client
	rm -rf server/IM_server



