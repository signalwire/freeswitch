HTTP/1.0 200 OK
Content-Type: text/html

<html><body>

<p>
<span>Today is:</span>
<? mg.write(os.date("%A")) ?>
</p>
<p>
URI is <?=mg.request_info.uri?>
</p>

</body></html>
