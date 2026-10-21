// SPDX-License-Identifier: GPL-2.0-only
// Launcher texts follow the game language chosen in Settings. The English text is the key;
// a text without a translation, and Arabic, are shown in English. "{0}" and "{1}" are
// replaced by trf's arguments.
#pragma once
#include <map>
#include <string>
namespace launcher_text {
inline int language = 0;  // 0 English, 1 French, 2 Italian, 3 German, 4 Spanish
inline void set_language(const std::wstring& code) {
    language = code == L"fr" ? 1 : code == L"it" ? 2 : code == L"de" ? 3 : code == L"es" ? 4 : 0;
}
struct Entry { const wchar_t* fr; const wchar_t* it; const wchar_t* de; const wchar_t* es; };
inline const std::map<std::wstring, Entry>& table() {
    static const std::map<std::wstring, Entry> entries = {
        // Navigation and Home
        {L"HOME", {L"ACCUEIL", L"HOME", L"START", L"INICIO"}},
        {L"SETTINGS", {L"RÉGLAGES", L"OPZIONI", L"OPTIONEN", L"AJUSTES"}},
        {L"ADVANCED", {L"AVANCÉ", L"AVANZATE", L"ERWEITERT", L"AVANZADO"}},
        {L"PLAY!", {L"JOUER !", L"GIOCA!", L"SPIELEN!", L"¡JUGAR!"}},
        {L"RACING", {L"EN COURSE", L"IN GARA", L"IM RENNEN", L"EN CARRERA"}},
        {L"PLEASE WAIT", {L"PATIENTEZ", L"ATTENDI", L"BITTE WARTEN", L"ESPERA"}},
        {L"READY", {L"PRÊT", L"PRONTO", L"BEREIT", L"LISTO"}},
        {L"SETUP REQUIRED", {L"À CONFIGURER", L"DA CONFIGURARE", L"EINRICHTEN", L"CONFIGURAR"}},
        {L"Ready", {L"Prêt", L"Pronto", L"Bereit", L"Listo"}},
        {L"Fullscreen", {L"Plein écran", L"Schermo intero", L"Vollbild", L"Pantalla completa"}},
        {L"Check updates", {L"Mises à jour", L"Aggiornamenti", L"Updates suchen", L"Actualizaciones"}},
        {L"Checking...", {L"Vérification...", L"Verifica...", L"Prüfe...", L"Comprobando..."}},
        {L"Downloading...", {L"Téléchargement...", L"Download...", L"Herunterladen...", L"Descargando..."}},
        {L"Update available", {L"Mise à jour dispo", L"Aggiornamento pronto", L"Update verfügbar", L"Actualización lista"}},
        {L"Runtime not found: ctrport.exe and ctr_gpu.dll required", {L"Runtime introuvable : ctrport.exe et ctr_gpu.dll requis", L"Runtime non trovato: servono ctrport.exe e ctr_gpu.dll", L"Runtime fehlt: ctrport.exe und ctr_gpu.dll nötig", L"Runtime no encontrado: se necesitan ctrport.exe y ctr_gpu.dll"}},
        {L"Select valid dump folders to start the game", {L"Choisissez des dossiers de dump valides", L"Scegli cartelle del dump valide", L"Gültige Dump-Ordner wählen", L"Elige carpetas de dump válidas"}},
        {L"Invalid resolution (320×240 to 7680×4320)", {L"Résolution invalide (320×240 à 7680×4320)", L"Risoluzione non valida (da 320×240 a 7680×4320)", L"Ungültige Auflösung (320×240 bis 7680×4320)", L"Resolución no válida (320×240 a 7680×4320)"}},
        {L"Player name: 3-16 letters, digits, hyphen or underscore", {L"Nom : 3 à 16 lettres, chiffres, - ou _", L"Nome: da 3 a 16 lettere, cifre, - o _", L"Name: 3-16 Buchstaben, Ziffern, - oder _", L"Nombre: 3-16 letras, dígitos, - o _"}},
        {L"eboot.bin not found in the base folder", {L"eboot.bin introuvable dans le dossier de base", L"eboot.bin non trovato nella cartella base", L"eboot.bin fehlt im Basisordner", L"eboot.bin no está en la carpeta base"}},
        {L"Base game found", {L"Jeu de base trouvé", L"Gioco base trovato", L"Basisspiel gefunden", L"Juego base encontrado"}},
        {L"Update disabled: v01.00 will be started", {L"Mise à jour désactivée : lancement de la v01.00", L"Aggiornamento disattivato: si avvia la v01.00", L"Update aus: v01.00 wird gestartet", L"Actualización desactivada: se inicia la v01.00"}},
        {L"Title update found", {L"Mise à jour trouvée", L"Aggiornamento trovato", L"Update gefunden", L"Actualización encontrada"}},
        {L"Update folder or eboot.bin missing", {L"Dossier de mise à jour ou eboot.bin manquant", L"Cartella aggiornamento o eboot.bin mancante", L"Update-Ordner oder eboot.bin fehlt", L"Falta la carpeta de actualización o eboot.bin"}},
        // Settings
        {L"GRAPHICS", {L"GRAPHISMES", L"GRAFICA", L"GRAFIK", L"GRÁFICOS"}},
        {L"Display resolution:", {L"Résolution :", L"Risoluzione:", L"Auflösung:", L"Resolución:"}},
        {L"Internal 3D resolution:", {L"Résolution 3D :", L"Risoluzione 3D:", L"3D-Auflösung:", L"Resolución 3D:"}},
        {L"Scene upscaling:", {L"Mise à l'échelle :", L"Upscaling:", L"Hochskalierung:", L"Escalado:"}},
        {L"Network mode:", {L"Réseau :", L"Rete:", L"Netzwerk:", L"Red:"}},
        {L"Player name:", {L"Nom du joueur :", L"Nome giocatore:", L"Spielername:", L"Nombre:"}},
        {L"CONTROLLER", {L"MANETTE", L"CONTROLLER", L"CONTROLLER", L"MANDO"}},
        {L"Game language:", {L"Langue du jeu :", L"Lingua del gioco:", L"Spielsprache:", L"Idioma del juego:"}},
        {L"Custom", {L"Autre", L"Altro", L"Eigene", L"Otra"}},
        {L"Fullscreen Mode", {L"Plein écran", L"Schermo intero", L"Vollbild", L"Pantalla completa"}},
        {L"Multiplayer Online", {L"Multijoueur en ligne", L"Multiplayer online", L"Online-Mehrspieler", L"Multijugador online"}},
        {L"Colored Xbox icons", {L"Icônes Xbox", L"Icone Xbox colorate", L"Xbox-Symbole", L"Iconos Xbox"}},
        {L"Split screen (2-4)", {L"Écran partagé (2-4)", L"Schermo diviso (2-4)", L"Splitscreen (2-4)", L"Pantalla dividida (2-4)"}},
        {L"Test audio", {L"Tester l'audio", L"Prova audio", L"Audio testen", L"Probar audio"}},
        // Advanced
        {L"Base game folder (CUSA14876 v01.00):", {L"Dossier du jeu de base (CUSA14876 v01.00) :", L"Cartella del gioco base (CUSA14876 v01.00):", L"Basisspiel-Ordner (CUSA14876 v01.00):", L"Carpeta del juego base (CUSA14876 v01.00):"}},
        {L"Update folder (v01.21 CUSA14876-patch):", {L"Dossier de mise à jour (v01.21 CUSA14876-patch) :", L"Cartella aggiornamento (v01.21 CUSA14876-patch):", L"Update-Ordner (v01.21 CUSA14876-patch):", L"Carpeta de actualización (v01.21 CUSA14876-patch):"}},
        {L"SESSION LOG", {L"JOURNAL", L"LOG SESSIONE", L"SITZUNGSLOG", L"REGISTRO"}},
        {L"No session yet.", {L"Aucune session.", L"Nessuna sessione.", L"Noch keine Sitzung.", L"Ninguna sesión todavía."}},
        {L"Waiting for the game...", {L"En attente du jeu...", L"In attesa del gioco...", L"Warte auf das Spiel...", L"Esperando al juego..."}},
        {L"BROWSE", {L"PARCOURIR", L"SFOGLIA", L"WÄHLEN", L"EXAMINAR"}},
        {L"Back up saves", {L"Sauvegarder", L"Backup salvataggi", L"Spielstände sichern", L"Copiar partidas"}},
        {L"Open saves", {L"Ouvrir les sauvegardes", L"Apri salvataggi", L"Spielstände öffnen", L"Abrir partidas"}},
        {L"Open logs folder", {L"Ouvrir les journaux", L"Apri cartella log", L"Log-Ordner öffnen", L"Abrir registros"}},
        {L"Base game folder (CUSA14876)", {L"Dossier du jeu de base (CUSA14876)", L"Cartella del gioco base (CUSA14876)", L"Basisspiel-Ordner (CUSA14876)", L"Carpeta del juego base (CUSA14876)"}},
        {L"Update folder (v01.21)", {L"Dossier de mise à jour (v01.21)", L"Cartella aggiornamento (v01.21)", L"Update-Ordner (v01.21)", L"Carpeta de actualización (v01.21)"}},
        // Mods
        {L"REFRESH", {L"ACTUALISER", L"AGGIORNA", L"NEU LADEN", L"ACTUALIZAR"}},
        {L"OPEN FOLDER", {L"OUVRIR", L"APRI CARTELLA", L"ÖFFNEN", L"ABRIR"}},
        {L"FILE MODS", {L"MODS", L"MOD", L"MODS", L"MODS"}},
        {L"PATCHES", {L"PATCHS", L"PATCH", L"PATCHES", L"PARCHES"}},
        {L"Use mods and patches", {L"Activer mods et patchs", L"Usa mod e patch", L"Mods und Patches", L"Usar mods y parches"}},
        {L"{0} ENABLED", {L"{0} ACTIFS", L"{0} ATTIVI", L"{0} AKTIV", L"{0} ACTIVOS"}},
        {L"ALL OFF", {L"DÉSACTIVÉ", L"DISATTIVATI", L"AUS", L"DESACTIVADO"}},
        {L"No mods yet", {L"Aucun mod", L"Nessuna mod", L"Keine Mods", L"Ningún mod"}},
        {L"No patch files yet", {L"Aucun patch", L"Nessuna patch", L"Keine Patches", L"Ningún parche"}},
        {L"Folder: {0}", {L"Dossier : {0}", L"Cartella: {0}", L"Ordner: {0}", L"Carpeta: {0}"}},
        {L"Page {0} / {1}", {L"Page {0} / {1}", L"Pagina {0} / {1}", L"Seite {0} / {1}", L"Página {0} / {1}"}},
        {L"Game: {0}", {L"Jeu : {0}", L"Gioco: {0}", L"Spiel: {0}", L"Juego: {0}"}},
        {L"Game version unknown", {L"Version du jeu inconnue", L"Versione del gioco sconosciuta", L"Spielversion unbekannt", L"Versión del juego desconocida"}},
        {L"{0} game files", {L"{0} fichiers du jeu", L"{0} file del gioco", L"{0} Spieldateien", L"{0} archivos del juego"}},
        {L"No game files found", {L"Aucun fichier du jeu", L"Nessun file del gioco", L"Keine Spieldateien", L"Sin archivos del juego"}},
        {L"Not for this game: {0}", {L"Pas pour ce jeu : {0}", L"Non per questo gioco: {0}", L"Nicht für dieses Spiel: {0}", L"No es para este juego: {0}"}},
        {L"Unreadable patch files: {0}", {L"Patchs illisibles : {0}", L"Patch illeggibili: {0}", L"Unlesbare Patches: {0}", L"Parches ilegibles: {0}"}},
        {L"A mod cannot be loaded:", {L"Un mod ne peut pas être chargé :", L"Una mod non può essere caricata:", L"Ein Mod kann nicht geladen werden:", L"No se puede cargar un mod:"}},
        // Controllers
        {L"Player {0}", {L"Joueur {0}", L"Giocatore {0}", L"Spieler {0}", L"Jugador {0}"}},
        {L"Auto", {L"Auto", L"Auto", L"Auto", L"Auto"}},
        {L"None", {L"Aucun", L"Nessuno", L"Keiner", L"Ninguno"}},
        {L"Keyboard", {L"Clavier", L"Tastiera", L"Tastatur", L"Teclado"}},
        {L"Not connected", {L"Non connect\u00e9", L"Non collegato", L"Nicht verbunden", L"No conectado"}},
        {L"Xbox controller {0}", {L"Manette Xbox {0}", L"Controller Xbox {0}", L"Xbox-Controller {0}", L"Mando Xbox {0}"}},
        // Trophies
        {L"TROPHIES", {L"TROPH\u00c9ES", L"TROFEI", L"TROPH\u00c4EN", L"TROFEOS"}},
        {L"No trophy file in the dump", {L"Aucun fichier de troph\u00e9es dans le dump", L"Nessun file dei trofei nel dump", L"Keine Troph\u00e4endatei im Dump", L"No hay archivo de trofeos en el dump"}},
        {L"Trophy {0}", {L"Troph\u00e9e {0}", L"Trofeo {0}", L"Troph\u00e4e {0}", L"Trofeo {0}"}},
        {L"Hidden trophy", {L"Troph\u00e9e cach\u00e9", L"Trofeo nascosto", L"Verstecke Troph\u00e4e", L"Trofeo oculto"}},
        {L"Platinum", {L"Platine", L"Platino", L"Platin", L"Platino"}},
        {L"Gold", {L"Or", L"Oro", L"Gold", L"Oro"}},
        {L"Silver", {L"Argent", L"Argento", L"Silber", L"Plata"}},
        {L"Bronze", {L"Bronze", L"Bronzo", L"Bronze", L"Bronce"}},
        {L"Locked", {L"Verrouill\u00e9", L"Bloccato", L"Gesperrt", L"Bloqueado"}},
        {L"Unlocked {0}", {L"D\u00e9bloqu\u00e9 le {0}", L"Sbloccato il {0}", L"Freigeschaltet am {0}", L"Desbloqueado el {0}"}},
        {L"Trophy key (optional):", {L"Cl\u00e9 des troph\u00e9es (facultative) :", L"Chiave dei trofei (facoltativa):", L"Troph\u00e4en-Schl\u00fcssel (optional):", L"Clave de trofeos (opcional):"}},
        {L"Names and descriptions need your PS4 trophy key.", {L"Les noms et descriptions demandent votre cl\u00e9 de troph\u00e9es PS4.", L"Nomi e descrizioni richiedono la tua chiave dei trofei PS4.", L"Namen und Beschreibungen brauchen deinen PS4-Troph\u00e4en-Schl\u00fcssel.", L"Los nombres y descripciones necesitan tu clave de trofeos de PS4."}},
        {L"IMPORT FROM shadPS4", {L"IMPORTER DE shadPS4", L"IMPORTA DA shadPS4", L"AUS shadPS4", L"IMPORTAR DE shadPS4"}},
        {L"No trophy key in shadPS4's keys.json.", {L"Aucune cl\u00e9 de troph\u00e9es dans keys.json de shadPS4.", L"Nessuna chiave dei trofei nel keys.json di shadPS4.", L"Kein Troph\u00e4en-Schl\u00fcssel in der keys.json von shadPS4.", L"No hay clave de trofeos en el keys.json de shadPS4."}},
        // Friends
        {L"FRIENDS", {L"AMIS", L"AMICI", L"FREUNDE", L"AMIGOS"}},
        {L"COPY MY CODE", {L"COPIER MON CODE", L"COPIA IL MIO CODICE", L"MEINEN CODE KOPIEREN", L"COPIAR MI CÓDIGO"}},
        {L"Paste a friend's code", {L"Collez le code d'un ami", L"Incolla il codice di un amico", L"Code eines Freundes einfügen", L"Pega el código de un amigo"}},
        {L"ADD", {L"AJOUTER", L"AGGIUNGI", L"HINZUFÜGEN", L"AÑADIR"}},
        {L"REMOVE", {L"RETIRER", L"RIMUOVI", L"ENTFERNEN", L"QUITAR"}},
        {L"ADD A FRIEND", {L"AJOUTER UN AMI", L"AGGIUNGI UN AMICO", L"FREUND HINZUFÜGEN", L"AÑADIR UN AMIGO"}},
        {L"Online is off", {L"En ligne désactivé", L"Online disattivato", L"Online ist aus", L"En línea desactivado"}},
        {L"Connecting...", {L"Connexion...", L"Connessione...", L"Verbinde...", L"Conectando..."}},
        {L"Online service not started", {L"Service en ligne arrêté", L"Servizio online non avviato", L"Online-Dienst nicht gestartet", L"Servicio en línea no iniciado"}},
        {L"Online", {L"En ligne", L"Online", L"Online", L"En línea"}},
        {L"No friends yet.", {L"Aucun ami.", L"Nessun amico.", L"Noch keine Freunde.", L"Aún no hay amigos."}},
        {L"In game", {L"En jeu", L"In gioco", L"Im Spiel", L"En juego"}},
        {L"Online (game closed)", {L"En ligne (jeu fermé)", L"Online (gioco chiuso)", L"Online (Spiel geschlossen)", L"En línea (juego cerrado)"}},
        {L"Offline · must add your code", {L"Hors ligne · doit ajouter votre code", L"Offline · deve aggiungere il tuo codice", L"Offline · muss deinen Code hinzufügen", L"Desconectado · debe añadir tu código"}},
        {L"Offline", {L"Hors ligne", L"Offline", L"Offline", L"Desconectado"}},
        {L"Code copied. You must add each other.", {L"Code copié. Ajoutez-vous mutuellement.", L"Codice copiato. Dovete aggiungervi a vicenda.", L"Code kopiert. Ihr müsst euch gegenseitig hinzufügen.", L"Código copiado. Debéis añadiros mutuamente."}},
        {L"Could not copy the code.", {L"Impossible de copier le code.", L"Impossibile copiare il codice.", L"Code konnte nicht kopiert werden.", L"No se pudo copiar el código."}},
        {L"Invalid friend code.", {L"Code ami invalide.", L"Codice amico non valido.", L"Ungültiger Freundescode.", L"Código de amigo no válido."}},
        {L"This is your own code.", {L"C'est votre propre code.", L"Questo è il tuo codice.", L"Das ist dein eigener Code.", L"Es tu propio código."}},
        {L"Friend added.", {L"Ami ajouté.", L"Amico aggiunto.", L"Freund hinzugefügt.", L"Amigo añadido."}},
        {L"Remove {0} from friends?", {L"Retirer {0} des amis ?", L"Rimuovere {0} dagli amici?", L"{0} aus den Freunden entfernen?", L"¿Quitar a {0} de amigos?"}},
        {L"{0} removed.", {L"{0} retiré.", L"{0} rimosso.", L"{0} entfernt.", L"{0} quitado."}},
        {L"Wait for Online to connect.", {L"Attendez la connexion en ligne.", L"Attendi la connessione online.", L"Warte auf die Online-Verbindung.", L"Espera a la conexión en línea."}},
        // Dialogs
        {L"Updates", {L"Mises à jour", L"Aggiornamenti", L"Updates", L"Actualizaciones"}},
        {L"launcher_update.ps1 is missing next to the launcher.", {L"launcher_update.ps1 manque à côté du launcher.", L"Manca launcher_update.ps1 accanto al launcher.", L"launcher_update.ps1 fehlt neben dem Launcher.", L"Falta launcher_update.ps1 junto al launcher."}},
        {L"Automatic installation works only in the release folder.", {L"L'installation automatique ne fonctionne que dans le dossier de la version.", L"L'installazione automatica funziona solo nella cartella della release.", L"Die automatische Installation geht nur im Release-Ordner.", L"La instalación automática solo funciona en la carpeta de la versión."}},
        {L"Close the game and turn off Multiplayer Online first.", {L"Fermez d'abord le jeu et désactivez le multijoueur en ligne.", L"Prima chiudi il gioco e disattiva il multiplayer online.", L"Zuerst das Spiel schließen und Online-Mehrspieler ausschalten.", L"Primero cierra el juego y desactiva el multijugador online."}},
        {L"Release cannot be installed", {L"Version non installable", L"Release non installabile", L"Release nicht installierbar", L"Versión no instalable"}},
        {L"Installed version: {0}\nNew version: {1}", {L"Version installée : {0}\nNouvelle version : {1}", L"Versione installata: {0}\nNuova versione: {1}", L"Installierte Version: {0}\nNeue Version: {1}", L"Versión instalada: {0}\nNueva versión: {1}"}},
        {L"Download and install? The launcher restarts; saves, settings and dump are kept.", {L"Télécharger et installer ? Le launcher redémarre ; sauvegardes, réglages et dump sont conservés.", L"Scaricare e installare? Il launcher si riavvia; salvataggi, impostazioni e dump restano.", L"Herunterladen und installieren? Der Launcher startet neu; Spielstände, Einstellungen und Dump bleiben.", L"¿Descargar e instalar? El launcher se reinicia; se conservan partidas, ajustes y dump."}},
        {L"Update failed", {L"Échec de la mise à jour", L"Aggiornamento non riuscito", L"Update fehlgeschlagen", L"Error de actualización"}},
        {L"No newer version available.", {L"Aucune version plus récente.", L"Nessuna versione più recente.", L"Keine neuere Version.", L"No hay una versión más reciente."}},
        {L"DLSS unavailable", {L"DLSS indisponible", L"DLSS non disponibile", L"DLSS nicht verfügbar", L"DLSS no disponible"}},
        {L"DLSS needs ctr_dlss.dll and nvngx_dlss.dll next to ctrport.exe. Choose Off or AMD FSR 1.", {L"DLSS requiert ctr_dlss.dll et nvngx_dlss.dll à côté de ctrport.exe. Choisissez Off ou AMD FSR 1.", L"DLSS richiede ctr_dlss.dll e nvngx_dlss.dll accanto a ctrport.exe. Scegli Off o AMD FSR 1.", L"DLSS braucht ctr_dlss.dll und nvngx_dlss.dll neben ctrport.exe. Wähle Off oder AMD FSR 1.", L"DLSS necesita ctr_dlss.dll y nvngx_dlss.dll junto a ctrport.exe. Elige Off o AMD FSR 1."}},
        {L"No saves found.", {L"Aucune sauvegarde.", L"Nessun salvataggio.", L"Keine Spielstände.", L"No hay partidas."}},
        {L"Backup completed:", {L"Sauvegarde terminée :", L"Backup completato:", L"Sicherung abgeschlossen:", L"Copia completada:"}},
        {L"Save backup failed:", {L"Échec de la sauvegarde :", L"Backup dei salvataggi non riuscito:", L"Sicherung fehlgeschlagen:", L"Error al copiar las partidas:"}},
        {L"The game stopped (exit code {0}).", {L"Le jeu s'est arrêté (code {0}).", L"Il gioco si è fermato (codice {0}).", L"Das Spiel wurde beendet (Code {0}).", L"El juego se detuvo (código {0})."}},
    };
    return entries;
}
inline const wchar_t* tr(const wchar_t* english) {
    if (!english || !language) return english;
    const auto found = table().find(english);
    if (found == table().end()) return english;
    const Entry& e = found->second;
    const wchar_t* value = language == 1 ? e.fr : language == 2 ? e.it : language == 3 ? e.de : e.es;
    return value ? value : english;
}
inline std::wstring trf(const wchar_t* english, const std::wstring& first, const std::wstring& second = {}) {
    std::wstring value = tr(english);
    if (const auto at = value.find(L"{0}"); at != std::wstring::npos) value.replace(at, 3, first);
    if (const auto at = value.find(L"{1}"); at != std::wstring::npos) value.replace(at, 3, second);
    return value;
}
}
