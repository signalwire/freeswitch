if (session.ready()) {
   session.answer();
   session.speak("cepstral","David","Please wait while we refresh the RSS feeds.")

   fetchURLFile("http://weather.yahooapis.com/forecastrss?p=60610","rss/weather.rss");
   fetchURLFile("http://rss.news.yahoo.com/rss/topstories","rss/yahootop.rss");
   fetchURLFile("http://rss.news.yahoo.com/rss/science","rss/yahoosci.rss");
   fetchURLFile("http://rss.news.yahoo.com/rss/business","rss/yahoobus.rss");
   fetchURLFile("http://rss.news.yahoo.com/rss/entertainment","rss/yahooent.rss");
   fetchURLFile("http://rss.slashdot.org/Slashdot/slashdot","rss/slashdot.rss");
   fetchURLFile("http://www.freeswitch.org/xml.php","rss/freeswitch.rss");
}
