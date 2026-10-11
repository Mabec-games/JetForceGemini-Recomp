using System;
using System.IO;

namespace JfgLauncher
{
    internal static class ImportedRom
    {
        internal static string FileName(string profile)
        {
            return Path.Combine(LocalSetup.FullPath(profile), "roms", "jfg-us.z64");
        }
        internal static string Import(string profile, string source)
        {
            return ImportVerified(profile, source, LocalSetup.RomSize, LocalSetup.RomSha1);
        }
        // The production entry point pins the supported ROM. Synthetic tests
        // exercise the same copy/validation/atomic-publication path below.
        internal static string ImportVerified(string profile, string source, long size, string hash)
        {
            source = LocalSetup.FullPath(source);
            string destination = FileName(profile);
            string folder = Path.GetDirectoryName(destination);
            Directory.CreateDirectory(folder);
            if ((File.GetAttributes(folder) & FileAttributes.ReparsePoint) != 0 ||
                (File.Exists(destination) && (File.GetAttributes(destination) & FileAttributes.ReparsePoint) != 0))
                throw new InvalidDataException("The local ROM storage must be an ordinary folder and file.");
            string temporary = destination + "." + Guid.NewGuid().ToString("N") + ".tmp";
            try
            {
                using (var input = new FileStream(source, FileMode.Open, FileAccess.Read, FileShare.Read))
                {
                    LocalSetup.ValidateRom(input, size, hash);
                    if (String.Equals(source, destination, StringComparison.OrdinalIgnoreCase)) return destination;
                    using (var output = new FileStream(temporary, FileMode.CreateNew, FileAccess.ReadWrite, FileShare.None))
                    {
                        input.CopyTo(output);
                        output.Flush(true);
                        LocalSetup.ValidateRom(output, size, hash);
                    }
                }
                // A failed or interrupted import never replaces the prior valid copy.
                if (File.Exists(destination)) File.Replace(temporary, destination, null);
                else File.Move(temporary, destination);
                return destination;
            }
            finally { if (File.Exists(temporary)) File.Delete(temporary); }
        }
    }
}
