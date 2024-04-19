# Clipthumb

Clipthumb is a tool for extracting the embedded thumbanil .png from a Clip Studio Paint .clip file, which is embedded in an SQLite database.

## Features

- [x] Reads out the headers for each section of a .clip file, searching for the "SQLi" chunk
- [x] Saves the SQLite DB to a temporary file
- [x] Opens the SQLite DB and fetches the first "canvas preview" blob
- [x] Saves the binary contents to a file of your chosing (defaults to "output.png")
- [ ] Will save each "external" file to an appropriately named output file (I don't yet parse the names of each external item, and so have nowhere to save them)

