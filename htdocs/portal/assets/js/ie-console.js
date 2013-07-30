// var ie_console_alertFallback = true;
// var ie_console_divFallback = true;

if (typeof console === "undefined" || typeof console.log === "undefined") {
 console = {};

 if (typeof ie_console_divFallback != "undefined") {
    console.log = function(msg) {
      $('#ie_console_debug_div').append(msg);
      $('#ie_console_debug_div').append("<br>");
    }
 } else if (typeof ie_console_alertFallback != "undefined" ) {
     console.log = function(msg) {
          alert(msg);
     };
 } else {
     console.log = function() {};
 }
}
