using System.Globalization;
using System.IO;
using System.Linq;
using System.Reflection;
using System.Collections.Generic;
using System.Xml;
using System.Xml.Linq;

namespace LEPContextMenuHandler
{
    internal class I18n
    {
        internal static readonly CultureInfo CurrentCultureInfo = CultureInfo.CurrentUICulture;
        //internal static readonly CultureInfo CurrentCultureInfo = CultureInfo.GetCultureInfo("zh-CN");

        private static XDocument cacheDictionary;

        internal static string GetString(string key)
        {
            var dict = LoadDictionary();
            try
            {
                var strings = from i in dict.Descendants("Strings").Elements() select i;

                var str = (from s in strings where s.Name == key select s.Value).FirstOrDefault();

                if (string.IsNullOrEmpty(str))
                    return key;

                return str;
            }
            catch
            {
                return key;
            }
        }

        private static XDocument LoadDictionary()
        {
            if (cacheDictionary != null)
                return cacheDictionary;

            var dictionary = LoadExternalDictionary() ?? LoadEmbeddedDictionary();

            //If dictionary is still null, use default language.
            if (dictionary == null)
                dictionary = XDocument.Load(new XmlTextReader(new StringReader(Resource.DefaultLanguage)));

            cacheDictionary = dictionary;

            return cacheDictionary;
        }

        private static XDocument LoadExternalDictionary()
        {
            try
            {
                var langDir = Path.Combine(Path.GetDirectoryName(typeof(I18n).Assembly.Location), "Lang");

                foreach (var cultureName in GetCandidateCultureNames())
                {
                    var langPath = Path.Combine(langDir, cultureName + ".xml");

                    if (File.Exists(langPath))
                        return XDocument.Load(langPath);
                }
            }
            catch
            {
            }

            return null;
        }

        private static XDocument LoadEmbeddedDictionary()
        {
            try
            {
                var assembly = typeof(I18n).Assembly;

                foreach (var cultureName in GetCandidateCultureNames())
                {
                    var resourceName = "LEPContextMenuHandler.Lang." + cultureName + ".xml";

                    using (var stream = assembly.GetManifestResourceStream(resourceName))
                    {
                        if (stream == null)
                            continue;

                        return XDocument.Load(stream);
                    }
                }
            }
            catch
            {
            }

            return null;
        }

        private static IEnumerable<string> GetCandidateCultureNames()
        {
            if (!string.IsNullOrEmpty(CurrentCultureInfo.Name))
                yield return CurrentCultureInfo.Name;

            if (CurrentCultureInfo.Name == "zh-Hans" || CurrentCultureInfo.Name == "zh-CHS")
                yield return "zh-CN";
            else if (CurrentCultureInfo.Name == "zh-Hant" || CurrentCultureInfo.Name == "zh-CHT")
                yield return "zh-TW";

            if (!string.IsNullOrEmpty(CurrentCultureInfo.TwoLetterISOLanguageName))
                yield return CurrentCultureInfo.TwoLetterISOLanguageName;
        }
    }
}
