using System.Security.Cryptography;

namespace Editor;

internal static class PackSignature
{
    // Public verification key from Sven2.exe VA 0x446558 (DSS1 PUBLICKEYBLOB).
    // SHA-256: 49d1731a828ad2d963bed165546371a08c6d04f0bef46f6f4d716e4c947499fe.
    private const string KeyBlob = "BgIAAAAiAABEU1MxAAQAAOP6HRhV0CSC/BVaKh1jFofTL+YvISoKiaf5h5V1gYHl9XguQoTnILwauRC4tE1U0MB5neF5DjlIK4QeWQk9rO71xWh7caV4Zv3MIGkUsg10R5+TbpQj8lw1K8HJSWSOTDbAb4zwkejcNHf+UWtT2Oie3e3nGlVVP62ngw5ijNraGVWJckbouc4gMbRiv0dP4j3NM5J155sAa9RvOV9bwo8kNwiKR2TBhAgj41SW4e8s9tqUCVQgYv2mEaPcPJr/WwGDkBo+q8L5KjxrsEikfZ+/EFyB0TWygT+PNnIIglOf3mWGSNkbbZffK1M5zUt/QDCvIiSGb+iAB9O51YBlG6WL9YCIsOnRdonkGAfDfmq2Q/RsD69MgXCV7lcbiUv4oI+xrd5MhFdhJYBbt23nY4aRE8V87y4FmxGxJpPtKPg2ZyVYO7/lQeL35Y4VQ6KFcawCN6Gpj9389JistBjuNpNr1+zJHnbJdJVCB3k9nEuP1/Ucux0DvkCS4NaFO/IUETPBMVw+zfVQlJ/+7/vrKyxneZqFrwAAACb23U54VlcJwEqOq8yiaTuGHdfp";

    public static string Verify(byte[] data)
    {
        if (data.Length < 44) return "Signature: missing or unrecognized trailer.";
        uint length = BitConverter.ToUInt32(data, data.Length - 4);
        if (length != 40) return "Signature: missing or unrecognized trailer (expected 40-byte DSA signature).";
        int start = data.Length - 44;
        byte[] key = Convert.FromBase64String(KeyBlob);
        byte[] BigEndian(int offset, int size) => key.AsSpan(offset, size).ToArray().Reverse().ToArray();
        byte[] signature = data.AsSpan(start, 40).ToArray();
        Array.Reverse(signature, 0, 20); Array.Reverse(signature, 20, 20);
        try
        {
            using DSA dsa = DSA.Create();
            dsa.ImportParameters(new DSAParameters { P = BigEndian(16,128), Q = BigEndian(144,20), G = BigEndian(164,128), Y = BigEndian(292,128) });
            bool valid = dsa.VerifyData(data.AsSpan(0,start), signature, HashAlgorithmName.SHA1, DSASignatureFormat.IeeeP1363FixedFieldConcatenation);
            return valid ? "Signature: valid under the game's publisher key." : "Signature: invalid under the game's publisher key (modified content or different key).";
        }
        catch (CryptographicException) { return "Signature: verification could not be completed by this system."; }
    }
}
