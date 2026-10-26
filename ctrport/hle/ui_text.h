// SPDX-License-Identifier: GPL-2.0-only
// Texts of the runtime's own windows (invitation picker, prompts, trophy notice), in the game language
// chosen in the launcher (CTR_SYSTEM_LANGUAGE: 1 English, 2 French, 3 Spanish, 4 German,
// 5 Italian). Other languages use English. "{0}" is replaced by ui_format's argument.
#pragma once
#include <windows.h>
#include <cstdlib>
#include <string>
namespace ctr_ui {
enum class Text { NoFriends, Send, InvitationFrom, NoInvitations, Accept, JoinQuestion, AcceptFailed,
                  DeclineFailed, RetryHint, ErrorTitle, SelectRecipients, Cancel, TrophyUnlocked, Trophy,
                  ExitTitle, ExitQuestion, ExitConfirm, ExitCancel, Count };
// The picker window is found by this title (invitation_dialog.cpp), so it is not translated.
inline constexpr const wchar_t* kInvitationTitle = L"CTR - Invitation";
inline int ui_language() {
    char value[8]{};
    const DWORD n = GetEnvironmentVariableA("CTR_SYSTEM_LANGUAGE", value, sizeof(value));
    const int id = n && n < sizeof(value) ? std::atoi(value) : 1;
    switch (id) { case 2: return 1; case 3: return 4; case 4: return 3; case 5: return 2; default: return 0; }
}
inline const wchar_t* ui_text(Text text) {
    // English, French, Italian, German, Spanish
    static constexpr const wchar_t* table[static_cast<int>(Text::Count)][5] = {
        {L"No friends available.", L"Aucun ami disponible.", L"Nessun amico disponibile.", L"Keine Freunde verfügbar.", L"No hay amigos disponibles."},
        {L"Send", L"Envoyer", L"Invia", L"Senden", L"Enviar"},
        {L"Invitation from {0}", L"Invitation de {0}", L"Invito da {0}", L"Einladung von {0}", L"Invitación de {0}"},
        {L"No pending invitations.", L"Aucune invitation en attente.", L"Nessun invito in attesa.", L"Keine ausstehenden Einladungen.", L"No hay invitaciones pendientes."},
        {L"Accept", L"Accepter", L"Accetta", L"Annehmen", L"Aceptar"},
        {L"Invitation from {0}. Join the session?", L"Invitation de {0}. Rejoindre la session ?", L"Invito da {0}. Entrare nella sessione?", L"Einladung von {0}. Der Sitzung beitreten?", L"Invitación de {0}. ¿Unirse a la sesión?"},
        {L"Could not accept the invitation. You did not join the session.", L"Impossible d'accepter l'invitation. Vous n'avez pas rejoint la session.", L"Impossibile accettare l'invito. Non sei entrato nella sessione.", L"Die Einladung konnte nicht angenommen werden. Du bist der Sitzung nicht beigetreten.", L"No se pudo aceptar la invitación. No te has unido a la sesión."},
        {L"Could not decline the invitation.", L"Impossible de refuser l'invitation.", L"Impossibile rifiutare l'invito.", L"Die Einladung konnte nicht abgelehnt werden.", L"No se pudo rechazar la invitación."},
        {L"Retry repeats the operation. Cancel stops the notices for this invitation.", L"Réessayer répète l'opération. Annuler arrête les avis pour cette invitation.", L"Riprova ripete l'operazione. Annulla interrompe gli avvisi per questo invito.", L"Wiederholen führt den Vorgang erneut aus. Abbrechen beendet die Hinweise zu dieser Einladung.", L"Reintentar repite la operación. Cancelar detiene los avisos de esta invitación."},
        {L"CTR - Invitation error", L"CTR - Erreur d'invitation", L"CTR - Errore invito", L"CTR - Einladungsfehler", L"CTR - Error de invitación"},
        {L"Select 1 to {0} recipients.", L"Sélectionnez de 1 à {0} destinataires.", L"Seleziona da 1 a {0} destinatari.", L"Wähle 1 bis {0} Empfänger.", L"Selecciona de 1 a {0} destinatarios."},
        {L"Cancel", L"Annuler", L"Annulla", L"Abbrechen", L"Cancelar"},
        {L"Trophy unlocked", L"Trophée débloqué", L"Trofeo sbloccato", L"Trophäe freigeschaltet", L"Trofeo desbloqueado"},
        {L"Trophy {0}", L"Trophée {0}", L"Trofeo {0}", L"Trophäe {0}", L"Trofeo {0}"},
        {L"Quit the game", L"Quitter le jeu", L"Esci dal gioco", L"Spiel beenden", L"Salir del juego"},
        {L"Close the game?\nWait for any saving to finish.\nRelease the buttons, then press A to quit or B to cancel.",
         L"Fermer le jeu ?\nAttendez la fin des sauvegardes en cours.\nRelâchez les boutons, puis appuyez sur A pour quitter ou B pour annuler.",
         L"Vuoi chiudere il gioco?\nAttendi la fine di eventuali salvataggi.\nRilascia i pulsanti, poi premi A per uscire o B per annullare.",
         L"Spiel schließen?\nWarte, bis laufende Speichervorgänge beendet sind.\nLass die Tasten los und drücke dann A zum Beenden oder B zum Abbrechen.",
         L"¿Cerrar el juego?\nEspera a que terminen los guardados en curso.\nSuelta los botones y pulsa A para salir o B para cancelar."},
        {L"A - Quit", L"A - Quitter", L"A - Esci", L"A - Beenden", L"A - Salir"},
        {L"B - Cancel", L"B - Annuler", L"B - Annulla", L"B - Abbrechen", L"B - Cancelar"},
    };
    return table[static_cast<int>(text)][ui_language()];
}
inline std::wstring ui_format(Text text, const std::wstring& argument) {
    std::wstring value = ui_text(text);
    const auto at = value.find(L"{0}");
    if (at != std::wstring::npos) value.replace(at, 3, argument);
    return value;
}
}
