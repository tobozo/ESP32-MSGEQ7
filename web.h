
#include <WiFi.h>
#include <WiFiClient.h>
#include <WebServer.h>
#include <ESPmDNS.h>
/*\
  cd ~/.arduino15/packages/esp32/hardware/esp32/1.0.4/libraries
  rm -Rf WebServer
  svn checkout https://github.com/espressif/arduino-esp32/trunk/libraries/WebServer
\*/
#include <uri/UriBraces.h>
#include <uri/UriRegex.h>

WebServer server(80);

bool webServerRunning = false;

const char* indexPage = ""
  "<!doctype html>"
  "<html>"
  "<head>"
  "<meta charset='utf-8' />"
  "<link rel='stylesheet' href='/index.css' />"
  "<style>"
  "input[type=range] {"
  "  transform: translate(0.5rem, 160px) rotate( -90deg );"
  "  transform-origin: center left;"
  "  position: absolute;"
  "}"
  ".range-holder {"
  "  display: inline-block;"
  "  min-width: 5em;"
  "  text-align: center;"
  "  font-size: .55em;"
  "  font-family: arial;"
  "}"
  "[data-freq]::after {"
  "  content: attr( data-freq );"
  "}"
  "</style>"
  "</head>"
  "<body>"
  "<div>"
  "  <div class='range-holder' data-freq='63 Hz'   ><input type='range' value='%d'></div>"
  "  <div class='range-holder' data-freq='160 Hz'  ><input type='range' value='%d'></div>"
  "  <div class='range-holder' data-freq='400 Hz'  ><input type='range' value='%d'></div>"
  "  <div class='range-holder' data-freq='1 KHz'   ><input type='range' value='%d'></div>"
  "  <div class='range-holder' data-freq='2.5KHz'  ><input type='range' value='%d'></div>"
  "  <div class='range-holder' data-freq='6.25 KHz'><input type='range' value='%d'></div>"
  "  <div class='range-holder' data-freq='16 KHz'  ><input type='range' value='%d'></div>"
  "</div>"
  "<script>"
  "const rangeHolders = document.querySelectorAll('input[type=range]');"
  "function rangeChangedHandler(event) {"
  "  const apiurl = '/level/' + event.target.getAttribute('data-index' ) + '/' + event.target.value;"
  "  top.location = apiurl;"
  "};"
  "rangeHolders.forEach( function( rangeHolder, index ) {"
  "  rangeHolder.setAttribute( 'data-index', index );"
  "  rangeHolder.addEventListener( 'change', rangeChangedHandler, false );"
  "});"
  "</script>"
  "</body>"
  "</html>"
;


int getPref( int band, const char* name="band" );
void setPref( int band, int value, const char* name="band"  );


void sendForm() {
  char temp[2048];
  //const char* tpl = "Band: %2d %2d %2d %2d %2d %2d %2d\n---------------------------------\nGain: %2d %2d %2d %2d %2d %2d %2d\n";

  snprintf( temp, 2048, indexPage,
    //0, 1, 2, 3, 4, 5, 6,
    getPref( 0 ),
    getPref( 1 ),
    getPref( 2 ),
    getPref( 3 ),
    getPref( 4 ),
    getPref( 5 ),
    getPref( 6 )
  );

  server.send(200, "text/html", temp );
}


void handleRoot() {

  int band    = atoi( server.pathArg(0).c_str() );
  int value   = atoi( server.pathArg(1).c_str() );

  if( value > 99 ) value = 99;

  //setSpectrumFactor( band, value );
  setPref( band, value );

  //sendForm();
  server.sendHeader("Location", "/", true);
  server.send(302, "text/plain", "");   // Empty content inhibits Content-length header so we have to close the socket ourselves.


}


void handleNotFound() {

  String message = "File Not Found\n\n";
  message += "URI: ";
  message += server.uri();
  message += "\nMethod: ";
  message += (server.method() == HTTP_GET) ? "GET" : "POST";
  message += "\nArguments: ";
  message += server.args();
  message += "\n";

  for (uint8_t i = 0; i < server.args(); i++) {
    message += " " + server.argName(i) + ": " + server.arg(i) + "\n";
  }

  server.send(404, "text/plain", message);

}
