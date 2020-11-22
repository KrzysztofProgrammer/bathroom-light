# bathroom-light
Sterowanie oświetleniem w łazience


## Czujniki
* PWM do obsługi ściemniacza LED
* sygnał z włącznika na ścianie (monostabilny)
* włączanie reflektorów - 4 pozycje
* czujnik ruchu
* temperatura podłogi


* temperatura i wilgotność - sufit, wywietrznik
* temperatura i wilgotność - za oknem

# Logika
* Płynne załączenie 100% LED jeżeli kliknięto na światło, włączenie halogenów 
* Czujnik ruchu bez kliknięcia - płynne 20% LED 
* Wyłączenie halogenów  i LED jeżeli kliknięto i było coś włączone
* Wyłączenie halogenów i LED po 20 minutach jeżeli nie było wykrytego ruchu
