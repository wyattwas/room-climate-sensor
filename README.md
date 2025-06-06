# Anleitung ESP Raumsensoren

## Server

Alle notwendigen Services laufen auf einem Raspberry Pi 5, die IP-Adresse im WLAN IoT-WLAN ist 172.20.200.60.

Die Services werden über Docker verwaltet. Die Docker können über die Weboberfläche Portainer unter dem Port 9443
verwaltet werden.

| Service Name        | Port         | Aufgabe                                                                    | Logins                 |
|---------------------|--------------|----------------------------------------------------------------------------|------------------------|
| Raspberry Pi Server | SSH Port: 22 | Server                                                                     | pi, pi                 |
| Portainer           | 9443         | Docker mit Oberfläche verwalten                                            | admin, adminadminadmin |
| Apache              | 80           | Setllt die Website zur Verfügung die auf dem Monitor angezeigt werden soll |                        |
| Grafana             | 3000         | Stellt die grafische Darstellung der Daten zur Verfügung                   | admin, admin           |
| MariaDB             | 3306         | Stellt die Datenbank in die unsere Sensoren und Daten gespeichert werden   | root, root             |
| Mosquitto           | 1883         | MQTT Broker um die Daten zu verschicken                                    | admin, admin           |
| Node-RED            | 1880         | Empfängt die Daten von MQTT und schreibt sie in die MariaDB Datenbank      |                        |
| PHPAdmin            | 8080         | Weboberfläche um MariaDB zu verwalten                                      | root, root             |

Auf dem Raspberry Pi ist, im Verzeichnis `/home/pi/.config/autostart`, die Datei `autostart.desktop` abgelegt, die das Skript `/opt/autostart.sh` ausführt.
Das Skript öffnet nach 15 Sekunden Firefox, im Kino Modus, mit der URL `http://localhost`. Dadurch wird beim Start des Raspberries direkt die Website mit den Daten angezeigt.

## Clients

Der Code für die ESP Clients ist in einem GitHub Repository zu
finden: [github.com/wyattwas/room-climate-sensor](https://github.com/wyattwas/room-climate-sensor)
Der Code ist in C++ geschrieben und verwenden PlatformIO als Framework.

## Einen neuen Client hinzufügen

Wenn ein neuer ESP ins Netzwerk kommen soll, müssen folgende Schritte beachtet werden.

1. Repo clonen und Firmware auf den ESP spielen
2. Mac-Adresse des Gerätes auslesen und ins IoT-WLAN hinzufügen
3. Gerät in der `luftklimamesser` Datenbank in die Tabelle `sensor` hinzufügen

| Name                                       | Typ         | Null | Default |
|--------------------------------------------|-------------|------|---------|
| id (Mac Adresse) primary key               | varchar(50) | No   | No      |
| nr (Zahl die auf dem Gerät angebracht ist) | int(11)     | No   | No      |
| room (Der Raum in dem das Gerät hängt)     | text        | No   | No      |
| floor (Stockwerk)                          | text        | No   | No      |
| building (Gebäude)                         | text        | No   | No      |

4. Fertig! Das Gerät kann sich jetzt mit dem MQTT Broker verbinden und seine Daten werden in die Datenbank gespeichert.
