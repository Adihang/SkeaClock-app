package com.skeaclock.app;

import android.content.Context;
import android.content.SharedPreferences;
import android.content.res.Configuration;
import android.os.LocaleList;
import java.util.Locale;

/** Resolves and applies the app's persisted language selection. */
final class LocaleHelper {
  private static final String SELECTED_LANGUAGE = "selected_language";
  private static final String AUTO = "auto";

  private LocaleHelper() {}

  static Context onAttach(Context context) {
    return withLocale(context, locale(context));
  }

  static Context setLocale(Context context, String language) {
    String normalized = normalize(language);
    Store.prefs(context).edit().putString(SELECTED_LANGUAGE, normalized).apply();
    return withLocale(context, resolve(normalized));
  }

  static String selectedLanguage(Context context) {
    SharedPreferences appPreferences = Store.prefs(context);
    if (appPreferences.contains(SELECTED_LANGUAGE))
      return normalize(appPreferences.getString(SELECTED_LANGUAGE, AUTO));

    // Migrate values saved by the previous AndroidX PreferenceManager implementation.
    SharedPreferences legacy =
        context.getSharedPreferences(context.getPackageName() + "_preferences", Context.MODE_PRIVATE);
    if (legacy.contains(SELECTED_LANGUAGE)) {
      String value = normalize(legacy.getString(SELECTED_LANGUAGE, AUTO));
      appPreferences.edit().putString(SELECTED_LANGUAGE, value).apply();
      return value;
    }
    return AUTO;
  }

  static Locale locale(Context context) {
    return resolve(selectedLanguage(context));
  }

  private static String normalize(String language) {
    if (language == null || language.isEmpty() || AUTO.equals(language)) return AUTO;
    String code = Locale.forLanguageTag(language).getLanguage().toLowerCase(Locale.ROOT);
    return code.equals("ko") || code.equals("ja") || code.equals("en") ? code : AUTO;
  }

  private static Locale resolve(String selected) {
    if (!AUTO.equals(selected)) return Locale.forLanguageTag(selected);
    LocaleList systemLocales = LocaleList.getDefault();
    for (int index = 0; index < systemLocales.size(); index++) {
      String language = systemLocales.get(index).getLanguage();
      if (language.equals("ko") || language.equals("ja") || language.equals("en"))
        return systemLocales.get(index);
    }
    return Locale.ENGLISH;
  }

  private static Context withLocale(Context context, Locale locale) {
    Configuration configuration = new Configuration(context.getResources().getConfiguration());
    configuration.setLocale(locale);
    configuration.setLocales(new LocaleList(locale));
    return context.createConfigurationContext(configuration);
  }
}
