# InstaFollowCount

![InstaFollowCount on TTGO T-Display](InstaFollowCount-T-Display.jpg?raw=true)

Use the GraphQL to get the follower count of an Instagram account on an ESP32.

Uses [ArduinoJson](https://arduinojson.org/) to parse the data, but could be done via string manipulation to save some space.

Sample with Display for [TTGO T-Display](https://github.com/Xinyuan-LilyGO/TTGO-T-Display).

Fallback to use [GreatFon](https://greatfon.com/), [DUMPOR](https://dumpor.com/) or [AnonIGViewer](https://www.anonigviewer.com/) in case Instagram's redirect-to-login rate limit kicks in. Very basic way of parsing the html but it did work :-)

This is just a proof of concept, not intendet to be used regularly / in production.

# Graphic sample

![InstaFollowCount with images on TTGO T-Display](InstaFollowCount-T-Display-catinthedicebag.JPG?raw=true)

Image used is from [CatInTheDiceBag](https://www.etsy.com/de/shop/CatInTheDiceBag) because Kim asked me something in order to build a Follower-IoT-Thingy, now we use Daniel's dices as sample. I do not know Daniel in person, neither have I ever bought any of his cool dices or gotten any discount from him. We just use him because I do not have an Instagram account myself.

# Interesting GraphQL query hashes for instagram found on the net:

## Following List
d04b0a864b4b54837c0d870b0e77e076

## Followers List
c76146de99bb02f6415203be841dd25a

## Story Viewers List
42c6ec100f5e57a1fe09be16cd3a7021

## Story List and First Fifty Viewers
52a36e788a02a3c612742ed5146f1676