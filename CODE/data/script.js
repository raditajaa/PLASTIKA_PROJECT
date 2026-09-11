// Polling status ke ESP32 setiap 500ms
setInterval(function () {
  fetch('/status')
    .then(function (r) { return r.json(); })
    .then(function (data) {
      var bar = document.getElementById('bar');
      bar.style.width = data.progress + '%';
      bar.innerText = data.progress + '%';

      document.getElementById('statusText').innerText = data.status;
      document.getElementById('valJarak').innerText = data.jarak + ' cm';
      document.getElementById('valJumlah').innerText = data.jumlah + ' Item';

      var btnDoor = document.getElementById('btnDoor');
      btnDoor.style.display = data.pintu ? 'block' : 'none';
    })
    .catch(function (err) {
      console.error('Gagal mengambil status:', err);
    });
}, 500);