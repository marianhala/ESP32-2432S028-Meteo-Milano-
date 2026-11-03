# Orologio e meteo Wi-Fi — ESP32-2432S028 (CYD)

Questo progetto mostra in **orizzontale** ora e data locali, temperatura, umidità, vento e descrizione del meteo sul display touchscreen della scheda. L'ora viene sincronizzata via NTP; il meteo viene letto da Open-Meteo ogni 15 minuti. Se la rete o il servizio non rispondono, il programma riprova ogni 30 secondi e mostra il codice di errore sul display. Non serve una chiave API. La località impostata è Roma.

## Rete Wi-Fi

Lo sketch è già configurato con:

- Rete: `M1`
- Password: `M1testM1`

Se cambi rete, modifica `WIFI_SSID` e `WIFI_PASSWORD` all'inizio di `OrarioMeteo/OrarioMeteo.ino`.

## Caricamento con Arduino IDE

1. Installa Arduino IDE e aggiungi il supporto **ESP32 by Espressif Systems** dal Gestore schede.
2. Installa queste librerie dal Gestore librerie: **TFT_eSPI** e **ArduinoJson**.
3. Apri `OrarioMeteo/OrarioMeteo.ino`.
4. Seleziona una scheda ESP32 compatibile, ad esempio **ESP32 Dev Module**, e la porta seriale della scheda.
5. Premi **Carica**. Al primo caricamento potrebbe essere necessario tenere premuto **BOOT** quando inizia la connessione, poi rilasciarlo.

Lo sketch usa la configurazione TFT_eSPI attiva sul computer. Per questa scheda il setup deve indicare il driver del display effettivo (la configurazione presente in questo ambiente è `ILI9341_2`) e i pin CYD: MOSI 13, SCLK 14, CS 15, DC 2 e retroilluminazione 21. Le revisioni ESP32-2432S028 possono montare controller diversi; se sposti il progetto su un altro computer, configura `User_Setup.h` di TFT_eSPI per la tua revisione.

## Compilazione da terminale

Dalla cartella del progetto:

```sh
arduino-cli compile --fqbn esp32:esp32:esp32 OrarioMeteo
```

Per caricare, sostituisci `/dev/cu.usbserial-XXXX` con la porta seriale corretta:

```sh
arduino-cli upload -p /dev/cu.usbserial-XXXX --fqbn esp32:esp32:esp32 OrarioMeteo
```

## Dati meteo e personalizzazione

Il servizio Open-Meteo richiede una connessione Internet. La richiesta usa coordinate di Roma (`41.9028, 12.4964`); per cambiare città, modifica latitudine e longitudine nell'URL `WEATHER_URL`. La scheda deve essere collegata a una rete Wi-Fi a 2,4 GHz.

I dati meteo sono forniti da [Open-Meteo](https://open-meteo.com/) e sono aggiornati ogni 15 minuti. Il certificato HTTPS del servizio viene accettato senza verifica nel firmware per semplificare l'uso su ESP32: utilizzare lo sketch su reti fidate.
