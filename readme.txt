STANCIOIU LAURA IOANA
322CD

Tema 2 PC
---------

Structura mesajului de la server catre subscriber
-------------------------------------------------
* mesajul este o structura de tip msg
* are ca header campul len(lungimea payloadului) si msg_type :
    - SUBSCRIBE : clientul vrea sa se aboneze la un topic
    - UNSUBSCRIBE : clientul vrea sa se dezaboneze de la un topic
    - UDP_PACKET : mesajul este de la un client udp
    - EXIT : clientul trebuie sa se inchida
    - NEW_CONNECTION : un client vrea sa se conecteze la server
    si ii trimite id-ul sau
* in payload se poate afla ori un sir de caractere(in cazul
subscribe, unsubscribe si new_connection), ori o structura tcp_packet
* tcp_packet contine portul, ip-ul si mesajul trimis de
catre clientul udp

Client TCP
----------

* Dupa ce clientul se conecteaza la server ii trimite un mesaj
cu id-ul sau

* Poate primii mesaje de la: - STDIN
                             - server

* STDIN:
    - comanda exit -> se iese din while si socketul clientului se inchide
    - comanda subscribe -> se trimite un mesaj de tip SUBSCRIBE serverului
                          cu numele topicului si valoarea flagului SF
                        -> se afiseaza Subscribed to topic.
    - comanda unsubscribe -> se trimite un mesaj de tip UNSUBSCRIBE
                            serverului cu numele topicului
                          -> se afiseaza Unsubscribed from topic.

* SERVER:
    - prima data se primeste headerul mesajului
    - mesaj cu tipul EXIT -> are acelasi efect ca si comanda exit primita de la
    stdin, inseamna ca serverul se va inchide sau clientul s-a conectat cu id-ul
    unui client deja existent
    - mesaj cu tipul UDP_PACKET -> s-a primit un mesaj de la un client udp
                                -> se face un recv pentru message.len bytes
                                in message.payload
                                -> daca numarul de bytes primiti este mai mic
                                decat message.len se mai face un recv pentru
                                restul mesajului
                                -> se extrag datele din mesaj si se afiseaza
                                la stdout

Server
------

* Serverul comunica cu : - clientii udp de la care primeste mesaje
                         - clientii tcp catre care trimite mesaje
                         - stdin, primeste comanda exit si inchide conexiunea

Mesaj NEW_CONNECTION
--------------------
* Daca se primeste un cerere de conexiune de la un client tcp aceasta
se accepta(i == sockfd)
* Urmand ca apoi sa se primeasca un mesaj de la clientul respectiv cu
id-ul sau (mesaj de tip NEW_CONNECTION)
* Daca id-ul clientului exista deja in tcp_clients se verifica daca
un client cu acelasi nume nu este deja conectat, caz in care se
trimite un mesaj de tip EXIT clientului curent si socketul este inchis,
altfel inseamna ca clientul s-a reconectat si i se trimit mesajele de la
topicurile marcate cu SF 1 la care este abonat si care au fost trimise
cat timp a fost deconectat
* Daca este vorba de un client, se completeaza datale pentru el si este
adaugat in tcp_clients

Mesaj SUBSCRIBE
---------------
* Se adauga in lista de subscriptii ale clientului noul topic
* Daca topicul nu exista deja in topics se adauga, clientul
este adaugat in lista de subscriberi
* Daca clientul era deja abonat la topic inseamna ca vrea sa isi
modifice valoarea flagului SF

Mesaj UNSUBSCRIBE
-----------------
* Topicul este eliminat din lista clientului de abonari
* Clientul este eliminat din lista de subscriberi ai topicului

Mesaj UDP_PACKET
----------------
* Se primeste mesajul de la clientul udp
* Daca topicul mesajului nu exista in topics se adauga
* Altfel, se construieste un mesaj si se trimite tuturor
subscriberilor acelui topic
* Daca vreunul dintre ei nu este conectat, dava are SF activ
pentru acel topic mesajul se stocheaza in lista acestuia de
mesaje ce ii vor fi trimise la reconectare