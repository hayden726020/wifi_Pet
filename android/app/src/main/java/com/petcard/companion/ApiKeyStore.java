package com.petcard.companion;

import android.content.Context;
import android.content.SharedPreferences;
import android.security.keystore.KeyGenParameterSpec;
import android.security.keystore.KeyProperties;
import android.util.Base64;

import java.nio.charset.StandardCharsets;
import java.security.KeyStore;

import javax.crypto.Cipher;
import javax.crypto.KeyGenerator;
import javax.crypto.SecretKey;
import javax.crypto.spec.GCMParameterSpec;

final class ApiKeyStore {
    private static final String ALIAS = "petcard.deepseek.api-key.v1";
    private static final String PREFS = "secure_provider_settings";
    private static final String KEY_CIPHER = "deepseek_key_ciphertext";
    private static final String KEY_IV = "deepseek_key_iv";
    private static final int GCM_TAG_BITS = 128;
    private final SharedPreferences preferences;

    ApiKeyStore(Context context) {
        preferences = context.getSharedPreferences(PREFS, Context.MODE_PRIVATE);
    }

    synchronized void save(String apiKey) throws Exception {
        String value = apiKey == null ? "" : apiKey.trim();
        if (value.length() < 16 || value.length() > 512) {
            throw new IllegalArgumentException("API key length is invalid");
        }
        Cipher cipher = Cipher.getInstance("AES/GCM/NoPadding");
        cipher.init(Cipher.ENCRYPT_MODE, getOrCreateKey());
        byte[] encrypted = cipher.doFinal(value.getBytes(StandardCharsets.UTF_8));
        boolean saved = preferences.edit()
                .putString(KEY_IV, Base64.encodeToString(cipher.getIV(), Base64.NO_WRAP))
                .putString(KEY_CIPHER, Base64.encodeToString(encrypted, Base64.NO_WRAP))
                .commit();
        if (!saved) throw new java.io.IOException("Could not save encrypted API key");
    }

    synchronized String read() throws Exception {
        String ivValue = preferences.getString(KEY_IV, null);
        String cipherValue = preferences.getString(KEY_CIPHER, null);
        if (ivValue == null || cipherValue == null) return null;
        byte[] iv = Base64.decode(ivValue, Base64.NO_WRAP);
        byte[] encrypted = Base64.decode(cipherValue, Base64.NO_WRAP);
        Cipher cipher = Cipher.getInstance("AES/GCM/NoPadding");
        cipher.init(Cipher.DECRYPT_MODE, getOrCreateKey(), new GCMParameterSpec(GCM_TAG_BITS, iv));
        return new String(cipher.doFinal(encrypted), StandardCharsets.UTF_8);
    }

    synchronized void clear() throws Exception {
        if (!preferences.edit().remove(KEY_IV).remove(KEY_CIPHER).commit()) {
            throw new java.io.IOException("Could not remove saved API key");
        }
        KeyStore keyStore = KeyStore.getInstance("AndroidKeyStore");
        keyStore.load(null);
        if (keyStore.containsAlias(ALIAS)) keyStore.deleteEntry(ALIAS);
    }

    boolean hasKey() {
        return preferences.contains(KEY_IV) && preferences.contains(KEY_CIPHER);
    }

    private SecretKey getOrCreateKey() throws Exception {
        KeyStore keyStore = KeyStore.getInstance("AndroidKeyStore");
        keyStore.load(null);
        if (keyStore.containsAlias(ALIAS)) {
            return ((KeyStore.SecretKeyEntry) keyStore.getEntry(ALIAS, null)).getSecretKey();
        }
        KeyGenerator generator = KeyGenerator.getInstance(KeyProperties.KEY_ALGORITHM_AES, "AndroidKeyStore");
        generator.init(new KeyGenParameterSpec.Builder(ALIAS,
                KeyProperties.PURPOSE_ENCRYPT | KeyProperties.PURPOSE_DECRYPT)
                .setBlockModes(KeyProperties.BLOCK_MODE_GCM)
                .setEncryptionPaddings(KeyProperties.ENCRYPTION_PADDING_NONE)
                .setRandomizedEncryptionRequired(true)
                .build());
        return generator.generateKey();
    }
}
