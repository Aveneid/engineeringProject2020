//Plik naglowkowy definiujacy numery pinow opoisanych na plytce z numerami pinow w mikrokontrolerze

#define D0 16
#define D1 5
#define D2 4
#define D3 0
#define D4 2
#define D5 14
#define D6 12
#define D7 13
#define D8 15


//WEBPAGE
const char indexHTML[] PROGMEM = R"=====(<!DOCTYPE html><html lang='en'><head><meta charset='utf-8'><meta name='viewport' content='width=device-width, initial-scale=1'><title>SmartLock - System zarządzania</title><link rel='stylesheet' href='https://maxcdn.bootstrapcdn.com/bootstrap/3.3.7/css/bootstrap.min.css'/><script src='https://ajax.googleapis.com/ajax/libs/jquery/1.12.4/jquery.min.js'></script><script src='https://maxcdn.bootstrapcdn.com/bootstrap/3.3.7/js/bootstrap.min.js'></script><script type='text/javascript'>function cD(id){if(confirm('Usunąć karte o ID: ' + id + '?')){console.log('delete?ID=' + id+':');window.location = 'delete?ID=' + id+':';}} function toggleNFC(){window.location ='toggleNFC';} function togglePIN(){window.location = 'togglePIN';} </script></head><body><div class='container-fluid'><div class='row'><div class='col-md-12'><div class='page-header'><h1>SmartLock <small>System zarządzania</small></h1></div><dl><dt>Panel administracyjny</dt><dd>W tym panelu możesz zarządzać zamkiem, sprawdzić rozpoznawane karty oraz ilość dostępnego miejsca dla nich. <br \>Możesz również zmienić ustawienia dostępu do zamka, zmienić PIN, włączyć / wyłączyć możliwość odczytu kart czy dostępu kodem PIN.</dd></dl><div class='row'><div class='col-md-7'><h3>Rozpoznawane karty:</h3><table class='table table-sm'><thead><tr><th>#</th><th>Identyfikator</th><th>Akcja</th></tr></thead><tbody>)=====";
const char indexHTML2[] PROGMEM = R"=====(</tbody></table> </div><div class='col-md-5'><h3>Dostępna pamięć kart:</h3><div class='progress'><div class='progress-bar' style='width:)=====";
const char indexHTML3[] PROGMEM = R"=====( % !important'></div></div><label>)=====";
const char indexHTML4[] PROGMEM = R"=====( % </label><form role='form' action='changePass'><div class='form-group'> <h4> Zmiana hasła administratora</h4><label for='pass'>Nowe hasło</label><input type='password' class='form-control' id='pass' name='PASS' maxlength='8'/></div><button type='submit' class='btn btn-primary'>Zmień</button></form><br /><form role='form' action='changePin'><div class='form-group'><h4> Zmiana kodu PIN</h4><label for='pin'>Nowy PIN</label><input type='text' class='form-control' id='pin' name='PIN' maxlength='8'/></div><button type='submit' class='btn btn-primary'>Zmień</button></form><br \><form role='form' action='changeLockTime'>  <div class='form-group'>  <h4> Zmiana czasu blokady</h4> <br /> <label for='pin'>Aktualny czas blokady:  )=====";
const char indexHTML5[] PROGMEM = R"=====( minut</label> <br /> <label for='locktime'>Nowy czas (max 255)</label>  <input class='form-control' type='number' min='1' max='255' id='locktime' name='LOCKTIME' maxlength='8'/>  </div>  <button type='submit' class='btn btn-primary'>Zmień</button>  </form><div class='checkbox'><label>)=====";
const char indexHTML6[] PROGMEM = R"=====(<a href='login?LOGOUT' class='btn btn-primary'> LOGOUT </a></div></div></div></div></div></div></body></html>)=====";
