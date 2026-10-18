package org.groupricochet.preview;

import android.app.Activity;
import android.app.AlertDialog;
import android.os.Bundle;
import android.text.InputType;
import android.view.WindowManager;
import android.widget.*;
import org.json.*;
import java.net.*;
import java.io.*;
import java.nio.charset.StandardCharsets;
import java.util.UUID;
import java.util.concurrent.ExecutorService;
import java.util.concurrent.Executors;

/** Native Android vertical slice against the SAME authoritative desktop test ledger.
 * No Tor chat, LiquidFeedback, or production issuance is claimed by this preview. */
public final class MainActivity extends Activity {
    private final ExecutorService io = Executors.newSingleThreadExecutor();
    private TextView status, summary, history;
    private EditText credential, asset, amount, recipient, name, cap;
    private LinearLayout content, admin;
    private Button retry, transfer, burn, mint, create;
    private String pending;
    private String pendingActor, actor = "", sessionToken = "";
    private boolean busy = false, connected = false, issuer = false;

    @Override public void onCreate(Bundle saved) {
        super.onCreate(saved);
        getWindow().setFlags(WindowManager.LayoutParams.FLAG_SECURE, WindowManager.LayoutParams.FLAG_SECURE);
        pending = getPreferences(MODE_PRIVATE).getString("pending", null);
        pendingActor = getPreferences(MODE_PRIVATE).getString("pendingActor", "");
        ScrollView scroll = new ScrollView(this);
        content = new LinearLayout(this); content.setOrientation(LinearLayout.VERTICAL);
        int pad = (int)(20 * getResources().getDisplayMetrics().density);
        content.setPadding(pad, pad, pad, pad); scroll.addView(content); setContentView(scroll);
        label("group-ricochet", 26);
        label("DAO · Preview locale", 20);
        label("Solo crediti di prova. Non denaro, non blockchain. Connessione al PC via USB (adb reverse).", 16);
        credential = field(content, "Credenziale personale del servizio locale", false);
        credential.setInputType(InputType.TYPE_CLASS_TEXT | InputType.TYPE_TEXT_VARIATION_PASSWORD);
        credential.setSaveEnabled(false);
        button(content, "Connetti / aggiorna", () -> request(false));
        status = label(pending == null ? "Non connesso" : "Operazione in sospeso: connetti e riprova con lo stesso ID.", 16);
        summary = label("Wallet · Nessun saldo caricato", 18);
        asset = field(content, "ID credito (es. DAO)", false);
        recipient = field(content, "ID destinatario accreditato", false);
        amount = field(content, "Quantità in unità intere", true);
        transfer = button(content, "Trasferisci", () -> prepare("transfer"));
        burn = button(content, "Brucia solo i miei crediti", () -> prepare("burn"));
        retry = button(content, "Riprova la stessa operazione", () -> request(true));
        admin = new LinearLayout(this); admin.setOrientation(LinearLayout.VERTICAL); content.addView(admin);
        TextView title = new TextView(this); title.setText("Amministrazione · Autorità di emissione"); admin.addView(title);
        mint = button(admin, "Emetti al destinatario", () -> prepare("mint"));
        name = field(admin, "Nome nuovo credito", false);
        cap = field(admin, "Limite totale di emissione", true);
        create = button(admin, "Crea credito con limite", () -> prepare("create"));
        label("Governance", 20);
        label("LiquidFeedback: integrazione non ancora attiva. Nessun voto o delega simulati.", 16);
        label("Chat", 20);
        label("La chat Tor Android non è ancora disponibile. La chat di gruppo resta nel client desktop.", 16);
        history = label("Storico personale", 16);
        updateButtons();
    }

    private TextView label(String text, int size) {
        TextView view = new TextView(this); view.setText(text); view.setTextSize(size);
        view.setPadding(0, 12, 0, 8); content.addView(view); return view;
    }
    private EditText field(LinearLayout parent, String hint, boolean numeric) {
        EditText view = new EditText(this); view.setHint(hint); view.setSingleLine(true);
        if (numeric) view.setInputType(InputType.TYPE_CLASS_NUMBER);
        parent.addView(view); return view;
    }
    private Button button(LinearLayout parent, String text, Runnable action) {
        Button view = new Button(this); view.setText(text); view.setOnClickListener(v -> action.run());
        parent.addView(view); return view;
    }
    private String value(EditText field) { return field.getText().toString().trim(); }
    private long positive(EditText field) {
        long number = Long.parseLong(value(field));
        if (number <= 0 || number > 9007199254740991L) throw new IllegalArgumentException();
        return number;
    }
    private void updateButtons() {
        boolean writable = !busy && connected && pending == null;
        transfer.setEnabled(writable); burn.setEnabled(writable);
        mint.setEnabled(writable && issuer); create.setEnabled(writable && issuer);
        retry.setEnabled(!busy && connected && pending != null && actor.equals(pendingActor));
        admin.setVisibility(issuer ? android.view.View.VISIBLE : android.view.View.GONE);
    }
    private void prepare(String kind) {
        if (busy || pending != null || !connected) return;
        try {
            JSONObject cmd = new JSONObject().put("kind", kind).put("asset", value(asset))
                .put("request_id", UUID.randomUUID().toString());
            if (kind.equals("create")) cmd.put("name", value(name)).put("cap", positive(cap));
            else cmd.put("amount", positive(amount));
            if (kind.equals("transfer") || kind.equals("mint")) cmd.put("to", value(recipient));
            new AlertDialog.Builder(this).setTitle("Conferma operazione di prova")
                .setMessage(cmd.toString(2)).setNegativeButton("Annulla", null)
                .setPositiveButton("Conferma", (dialog, which) -> {
                    pending = cmd.toString();
                    pendingActor = actor;
                    // Persist ID BEFORE submitting; a timeout must never cause a new payment ID.
                    if (!getPreferences(MODE_PRIVATE).edit().putString("pending", pending).putString("pendingActor", pendingActor).commit()) {
                        pending = null; status.setText("Impossibile salvare l'operazione; invio annullato."); return;
                    }
                    request(true);
                }).show();
        } catch (Exception e) { status.setText("Controlla ID, nome e quantità intere positive."); }
    }
    private void request(boolean write) {
        if (busy || (write && (pending == null || !connected || !actor.equals(pendingActor)))) return;
        final String token = write ? sessionToken : value(credential);
        if (token.length() < 32) { status.setText("Inserisci una credenziale personale valida (almeno 32 caratteri)."); return; }
        final String body = pending;
        busy = true; updateButtons(); status.setText("Connessione al servizio locale…");
        io.execute(() -> {
            HttpURLConnection connection = null;
            try {
                URL url = new URL("http://127.0.0.1:17842/v1/" + (write ? "commands" : "state"));
                connection = (HttpURLConnection)url.openConnection(Proxy.NO_PROXY);
                connection.setConnectTimeout(5000); connection.setReadTimeout(10000);
                connection.setInstanceFollowRedirects(false);
                connection.setRequestProperty("Authorization", "Bearer " + token);
                if (write) {
                    connection.setRequestMethod("POST"); connection.setDoOutput(true);
                    connection.setRequestProperty("Content-Type", "application/json");
                    try (OutputStream out = connection.getOutputStream()) { out.write(body.getBytes(StandardCharsets.UTF_8)); }
                }
                final int code = connection.getResponseCode();
                InputStream stream = code == 200 ? connection.getInputStream() : connection.getErrorStream();
                ByteArrayOutputStream bytes = new ByteArrayOutputStream();
                if (stream != null) try (InputStream in = stream) {
                    byte[] buffer = new byte[4096]; int n;
                    while ((n = in.read(buffer)) != -1) {
                        if (bytes.size() + n > 262144) throw new IOException("Response too large");
                        bytes.write(buffer, 0, n);
                    }
                }
                final JSONObject data = new JSONObject(new String(bytes.toByteArray(), StandardCharsets.UTF_8));
                runOnUiThread(() -> {
                    if (isFinishing() || isDestroyed()) return;
                    busy = false;
                    if (write && (code == 200 || (code == 400 && !data.optString("error").startsWith("Idempotency")))) {
                        // If removing the pending record fails, keep it for safe same-ID replay.
                        if (getPreferences(MODE_PRIVATE).edit().remove("pending").commit()) pending = null;
                    }
                    if (code == 200) {
                        status.setText(write ? "Operazione registrata. Ricevuta: " + data.optLong("sequence") : "Servizio locale connesso");
                        if (!write) { sessionToken = token; render(data); }
                    } else status.setText(data.optString("error", "Servizio non disponibile"));
                    updateButtons();
                    if (write && code == 200) request(false);
                });
            } catch (Exception e) {
                runOnUiThread(() -> {
                    if (isFinishing() || isDestroyed()) return;
                    busy = false; connected = false;
                    status.setText("Servizio non raggiungibile. Collega il PC via adb reverse. Se l'esito è incerto, riprova la stessa operazione.");
                    updateButtons();
                });
            } finally { if (connection != null) connection.disconnect(); }
        });
    }
    private void render(JSONObject state) {
        actor = state.optString("actor");
        connected = true; issuer = "issuer".equals(state.optString("role"));
        StringBuilder text = new StringBuilder("Wallet · ").append(state.optString("actor")).append("\n");
        JSONArray assets = state.optJSONArray("assets");
        if (assets != null) for (int i = 0; i < assets.length(); i++) {
            JSONObject item = assets.optJSONObject(i);
            text.append(item.optString("id")).append(" · saldo ").append(item.optLong("balance"))
                .append(" · emessi ").append(item.optLong("supply")).append(" / ").append(item.optLong("cap")).append("\n");
        }
        text.append("Destinatari: ").append(state.optJSONArray("members")); summary.setText(text);
        StringBuilder audit = new StringBuilder("Ultime operazioni personali\n");
        JSONArray events = state.optJSONArray("history");
        if (events != null) for (int i = 0; i < events.length(); i++) {
            JSONObject event = events.optJSONObject(i);
            audit.append("#").append(event.optLong("seq")).append(" · ").append(event.optString("body")).append("\n");
        }
        history.setText(audit);
    }
    @Override protected void onDestroy() { io.shutdownNow(); super.onDestroy(); }
}
