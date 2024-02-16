const fs = require("fs");
const https = require("https");
const express = require("express");
const bodyParser = require("body-parser");
const forge = require("node-forge");
const pki = forge.pki;

function createCACertificate() {
  let caPrivateKey, caKeys;

  // Check if the CA private key already exists, if not, generate a new one
  if (!fs.existsSync("ca-key.pem")) {
    caKeys = pki.rsa.generateKeyPair(2048); // Generate a new key pair
    caPrivateKey = caKeys.privateKey;
    const privateKeyPem = pki.privateKeyToPem(caPrivateKey);
    fs.writeFileSync("ca-key.pem", privateKeyPem);
    console.log("CA private key saved as ca-key.pem");
  } else {
    const caPrivateKeyPem = fs.readFileSync("ca-key.pem", "utf8");
    caPrivateKey = pki.privateKeyFromPem(caPrivateKeyPem);
    caKeys = {
      privateKey: caPrivateKey,
      publicKey: pki.rsa.setPublicKey(caPrivateKey.n, caPrivateKey.e),
    };
  }

  // Create a new CA certificate
  const caCert = pki.createCertificate();
  caCert.publicKey = caKeys.publicKey; // Use the public key from the key pair
  caCert.serialNumber = "01";
  caCert.validity.notBefore = new Date();
  caCert.validity.notAfter = new Date();
  caCert.validity.notAfter.setFullYear(
    caCert.validity.notBefore.getFullYear() + 10
  ); // 10 years validity

  const caAttrs = [
    {
      name: "commonName",
      value: "My CA",
    },
    {
      name: "countryName",
      value: "US",
    },
    {
      shortName: "ST",
      value: "Some-State",
    },
    {
      name: "localityName",
      value: "MyCity",
    },
    {
      name: "organizationName",
      value: "MyCompany",
    },
    {
      shortName: "OU",
      value: "MyCA",
    },
  ];

  caCert.setSubject(caAttrs);
  caCert.setIssuer(caAttrs); // Self-signed, so subject and issuer are the same

  caCert.setExtensions([
    {
      name: "basicConstraints",
      cA: true,
    },
    {
      name: "keyUsage",
      keyCertSign: true,
      cRLSign: true,
    },
  ]);

  // Sign the certificate with the CA's private key
  caCert.sign(caPrivateKey, forge.md.sha256.create());

  // Convert the CA certificate to PEM format and save it
  const caCertPem = pki.certificateToPem(caCert);
  fs.writeFileSync("ca.pem", caCertPem);
  console.log("CA certificate saved as ca.pem");
}

function createCertificate(csrPem = null) {
  let privateKey, publicKey;
  let csr;

  if (!csrPem) {
    // No CSR provided, generate a new key pair for the server
    if (!fs.existsSync("server-key.pem")) {
      const keys = pki.rsa.generateKeyPair(2048);
      privateKey = keys.privateKey;
      publicKey = keys.publicKey;

      // Save the server's private key
      const privateKeyPem = pki.privateKeyToPem(privateKey);
      fs.writeFileSync("server-key.pem", privateKeyPem);
      console.log("Server private key saved as server-key.pem");
    } else {
      // Load the server's existing private key
      const privateKeyPem = fs.readFileSync("server-key.pem", "utf8");
      privateKey = pki.privateKeyFromPem(privateKeyPem);
      publicKey = pki.rsa.setPublicKey(privateKey.n, privateKey.e);
    }
  } else {
    // CSR provided, extract the public key from the CSR
    csr = pki.certificationRequestFromPem(csrPem);
    if (!csr.verify()) {
      throw new Error("Invalid CSR.");
    }
    publicKey = csr.publicKey;
  }

  // Load the CA certificate and private key
  const caCertPem = fs.readFileSync("ca.pem", "utf8");
  const caCert = pki.certificateFromPem(caCertPem);
  const caPrivateKeyPem = fs.readFileSync("ca-key.pem", "utf8");
  const caPrivateKey = pki.privateKeyFromPem(caPrivateKeyPem);

  // Create a new certificate
  const cert = pki.createCertificate();
  cert.publicKey = publicKey;
  cert.serialNumber = "02";
  cert.validity.notBefore = new Date();
  cert.validity.notAfter = new Date();
  cert.validity.notAfter.setFullYear(cert.validity.notBefore.getFullYear() + 1); // 1 year validity

  const attrs = [
    {
      name: "commonName",
      value: csrPem ? csr.subject.getField("CN").value : "localhost",
    },
    {
      name: "countryName",
      value: "US",
    },
    {
      shortName: "ST",
      value: "Some-State",
    },
    {
      name: "localityName",
      value: "MyCity",
    },
    {
      name: "organizationName",
      value: "MyCompany",
    },
    {
      shortName: "OU",
      value: "MyApp",
    },
  ];

  cert.setSubject(attrs);
  cert.setIssuer(caCert.subject.attributes); // The issuer is the CA

  if (!csrPem) {
    // Add Key Usage and Extended Key Usage for server certificates
    cert.setExtensions([
      {
        name: "keyUsage",
        digitalSignature: true,
        keyEncipherment: true,
        dataEncipherment: true,
      },
      {
        name: "extKeyUsage",
        serverAuth: true,
      },
    ]);
  }

  // Sign the certificate with the CA's private key
  cert.sign(caPrivateKey, forge.md.sha256.create());

  // Convert the certificate to PEM format
  const certPem = pki.certificateToPem(cert);

  return certPem;
}

function initCrypto() {
  if (!fs.existsSync("ca.pem")) {
    createCACertificate();
  }

  if (!fs.existsSync("server-cert.pem")) {
    const serverCert = createCertificate();
    fs.writeFileSync("server-cert.pem", serverCert);
  }
}

initCrypto();

const app = express();
app.use(bodyParser.json());

const httpsOptions = {
  key: fs.readFileSync("server-key.pem"),
  cert: fs.readFileSync("server-cert.pem"),
  ca: fs.readFileSync("ca.pem"), // The CA certificate
  // Requesting the client's certificate for verification
  requestCert: true,
  rejectUnauthorized: false,
};

// Create the HTTPS server
const server = https.createServer(httpsOptions, app);

app.get("/csr", (req, res) => {
  res.json({ csr: "use POST" });
});

app.post("/csr", (req, res) => {
  const csrPem = req.body.csr;
  try {
    const clientCert = createCertificate(csrPem);
    res.json({ clientCert });
  } catch (err) {
    res.status(400).json({
      message: "Error creating the client certificate",
      error: err.message,
    });
  }
});

app.post("/test", (req, res) => {
  console.log("/test endpoint");

  if (req.client.authorized) {
    console.log("Client certificate is authorized.");

    const clientCert = req.socket.getPeerCertificate();

    // Check if the certificate object is not empty
    if (clientCert && Object.keys(clientCert).length) {
      console.log("Client Common Name:", clientCert.subject.CN);
    } else {
      console.log("Client certificate information is not available.");
    }
    // Client is authenticated; proceed with the operation
    const result = req.body.operand * 2;

    console.log("operand:", req.body.operand);
    console.log("result:", result);

    res.json({ result });
  } else {
    res.status(401).send("Unauthorized");
  }
});

server.listen(4443, () => {
  console.log("HTTPS server running on port 4443");
});
