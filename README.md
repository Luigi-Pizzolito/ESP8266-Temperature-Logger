# ESP8266 Temperature Logger (in development)
A simple wireless temperature logger. Publishes data via GET requests.

- [ESP8266 Temperature Logger](#esp8266-temperature-logger)
    - [Planned Features (And Progress)](#planned-features-and-progress)
    - [About](#about)
        - [Uploading Data](#uploading-data)
        - [Power Saving](#power-saving)
        - [Configuration](#configuration)
    - [Useful Links and References](#useful-links-and-references)

## Planned Features (And Progress)
 - [X] Uploads
 - [X] Power Saving
 - [ ] Configuration

## About
Designed to be simple and easy to use. Will simply log temperature at the specified interval and send the values over to a IOT Server.

### Uploading Data
To keep things simple, this data logger makes HTTP GET requests to upload it's data. It passes the temperature reading and supply voltage as arguments.

This makes for a extremely neat setup if it is paired with Google Forms, Google Sheets and IFTTT.
By opening up a Google Form, with text fields for temperature and voltage. You can link that form to a Google sheet, which automatically updates with every form response and can graph your data. Finally, submitting a form is done via a GET request, be it a complicated one. Therefore using IFTTT's maker webhooks. We can create an applet that takes a simple GET request from our ESP Datalogger and passes the values down to another request that submits the Google forms. Once this is set up, it works like a charm. Plus, live updates!

### Power Saving
Since this is a logger and will almost certainly spend most of it's time laying around not doing anything; we wan't to make it consume the least power as possible. This means that as soon as data is uploaded it immediatly goes back to deep sleep. Deep sleep turns all the ESP's internal modules off except for the RTC which wakes it back up. This plus removing the pesky red power led on a ESP-01 module gives excellent power saving.
But there's a catch, for the RTC to reset and wake up the ESP it needs an external interrupt. This means that we have to connect GPIO16(D0) to RESET. This would be easy on a ESP-12E, ESP-07 or NodeMCU but GPIO16 is not broken out on a ESP-01. So in order to access it you will have to attempt to solder a thin wire to the pin directly on the miniscule ESP chip. This is certainly the hardest part of making this logger, but it is do-able with just a standard soldering iron and a lot of patience.
By changing the interval between logs you can also improve the battery life. The longer the interval the longer it will last, but the fewer readings. You need to choose a interval large enough so that the power consumption is small, but small enough so that you can see any trends on the graph/data.

### Configuration
Still working on this, currently everything is hardcoded. Don't know if I will keep it that way.


## Useful Links and References
- ESP Info
    - Pinouts
        - [ESP-12 Pinout](https://esp8266.github.io/Arduino/versions/2.0.0/doc/esp12.png)
        - [ESP-01 Pinout](https://os.mbed.com/media/uploads/sschocke/esp8266-pinout_etch_copper_top.png)
    - General Guides
        - [Sparkfun Guide](https://learn.sparkfun.com/tutorials/esp8266-thing-hookup-guide/using-the-arduino-addon)
        - [Excellent Beginner's Guide](https://github.com/tttapa/ESP8266)
- GitHub Markdown Info
    - [Formatting and Syntax](https://help.github.com/articles/basic-writing-and-formatting-syntax/)
    - [Emoji Cheat Sheet](https://www.webpagefx.com/tools/emoji-cheat-sheet/)