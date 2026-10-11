using System;
using System.Collections.Generic;
using System.IO;
using System.Runtime.Serialization;
using System.Runtime.Serialization.Json;
using System.Security.Cryptography;

namespace JfgLauncher
{
    // The native frontend identifies its own path before spawning this embedded
    // helper. Resolve the package there, never in the helper extraction cache.
    internal static class PortablePackage
    {
        internal static readonly string[] RuntimeFiles = {
            "jfg-native-boot.exe", "SDL2.dll", "dxcompiler.dll", "dxil.dll",
            "concrt140.dll", "msvcp140.dll", "msvcp140_1.dll", "msvcp140_2.dll", "msvcp140_atomic_wait.dll", "msvcp140_codecvt_ids.dll", "vccorlib140.dll", "vcruntime140.dll", "vcruntime140_1.dll", "vcruntime140_threads.dll"
        };
        [DataContract]
        internal sealed class Entry
        {
            [DataMember(Name = "name", IsRequired = true)] public string Name = "";
            [DataMember(Name = "sha256", IsRequired = true)] public string Sha256 = "";
            [DataMember(Name = "size", IsRequired = true)] public long Size = 0;
        }
        [DataContract]
        internal sealed class Manifest
        {
            [DataMember(Name = "schema", IsRequired = true)] public int Schema = 0;
            [DataMember(Name = "version", IsRequired = true)] public string Version = "";
            [DataMember(Name = "files", IsRequired = true)] public Entry[] Files = null;
        }
        private static InvalidDataException Incomplete()
        {
            return new InvalidDataException("Game files are missing or changed. Extract the complete release ZIP into a new folder, then open its launcher.");
        }
        internal static string FromLauncher()
        {
            return Resolve(Environment.GetEnvironmentVariable("JFG_LAUNCHER_EXE"));
        }
        internal static string Resolve(string launcher)
        {
            if (String.IsNullOrWhiteSpace(launcher) || !File.Exists(launcher))
                throw new InvalidDataException("Open JFG-Launcher.exe from the extracted game folder.");
            string directory = Path.GetDirectoryName(LocalSetup.FullPath(launcher));
            string path = Path.Combine(directory, "jfg-package.json");
            if (!File.Exists(path) || new FileInfo(path).Length > 16384) throw Incomplete();
            Manifest manifest;
            try
            {
                using (var input = File.OpenRead(path))
                    manifest = (Manifest)new DataContractJsonSerializer(typeof(Manifest)).ReadObject(input);
            }
            catch (SerializationException) { throw Incomplete(); }
            if (manifest == null || manifest.Schema != 1 || String.IsNullOrWhiteSpace(manifest.Version) ||
                manifest.Files == null || manifest.Files.Length != RuntimeFiles.Length) throw Incomplete();
            var remaining = new HashSet<string>(RuntimeFiles, StringComparer.Ordinal);
            foreach (Entry entry in manifest.Files)
            {
                if (entry == null || !remaining.Remove(entry.Name ?? "") || entry.Size <= 0 ||
                    entry.Sha256 == null || !System.Text.RegularExpressions.Regex.IsMatch(entry.Sha256, "\\A[0-9a-f]{64}\\z")) throw Incomplete();
                string member = Path.Combine(directory, entry.Name);
                if (!File.Exists(member) || new FileInfo(member).Length != entry.Size ||
                    (File.GetAttributes(member) & FileAttributes.ReparsePoint) != 0) throw Incomplete();
                using (var input = File.OpenRead(member))
                using (var sha = SHA256.Create())
                {
                    string actual = BitConverter.ToString(sha.ComputeHash(input)).Replace("-", "").ToLowerInvariant();
                    if (actual != entry.Sha256) throw Incomplete();
                }
            }
            return LocalSetup.ValidateRuntime(Path.Combine(directory, "jfg-native-boot.exe"));
        }
    }
}
