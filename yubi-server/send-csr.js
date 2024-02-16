// From the current working directory, load the `csr.pem` file and POST it to the /csr endpoint of the server at https://localhost:4443.
// Use the `ca.pem` file to verify the server's certificate.
const fs = require("fs");
const https = require("https");
const path = require("path");
const fetch = require("node-fetch");

const httpsOptions = {
  ca: fs.readFileSync(path.join(__dirname, "ca.pem")),
};

const csrPem = fs.readFileSync(path.join(__dirname, "csr.pem"));

fetch("https://localhost:4443/csr", {
  method: "POST",
  headers: {
    "Content-Type": "application/json",
  },
  body: JSON.stringify({ csr: csrPem.toString() }),
  agent: new https.Agent(httpsOptions),
})
  .then((res) => res.json())
  .then((json) => {
    console.log(json);

    // Save the client certificate to a file
    fs.writeFileSync(path.join(__dirname, "client-cert.pem"), json.clientCert);
  })
  .catch((err) => console.error(err));
