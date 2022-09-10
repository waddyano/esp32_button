#include "index.h"

const char *index_page = R"!(<html>
	<head>
		<meta http-equiv="content-type" content="text/html; charset=utf-8" />
        <!-- <meta name="viewport" content="width=device-width, initial-scale=1.0"/> -->
		<title>ESP32 Bluetooth Button</title>
        <script>
function toggle() 
{
    var xhr = new XMLHttpRequest();
    xhr.open("GET", '/toggle');
    xhr.send();
}
function update_state()
{
    var xhr = new XMLHttpRequest();
    xhr.onreadystatechange = () => 
    {
        if (xhr.readyState === 4) 
        {
            stateval.innerHTML = xhr.response == "OFF" ? 'Off' : 'On';
        }
    }
    xhr.open("GET", '/state');
    xhr.send();
}
setInterval(update_state, 1000);
        </script>
        <style>
body, textarea, button {font-family: arial, sans-serif;}
h1 { text-align: center; }
.tabcenter { margin-left: auto; margin-right: auto; }
.center { max-width: 100%; max-height: 100vh; margin: auto; }
button { border: 0; border-radius: 0.3rem; background:#1fa3ec; color:#ffffff; line-height:2.4rem; font-size:1.2rem; width:180px;
-webkit-transition-duration:0.4s;transition-duration:0.4s;cursor:pointer;}
button:hover{background:#0b73aa;}
#state { line-height:2.4rem; font-size:1.2rem; text-align: center; }
.cb { border: 0; border-radius: 0.3rem; font-family: arial, sans-serif; color: black; line-height:2.4rem; font-size:1.2rem;}
        </style>
	</head>
	<body">
		<h1>ESP32 Bluetooth Button</h1>
        <table class="tabcenter">
            <tr><td id="state"><b id="stateval">On</b></td></tr>
            <tr><td><button onclick="toggle();">Toggle</button></td></tr>
            <tr><td><a href="/update"><button>Update</button></a></td></tr>
            <tr><td><a href="/restart"><button>Restart</button></a></td></tr>
        </table>
	</body>
</html>)!";
