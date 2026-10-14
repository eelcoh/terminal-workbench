on run arguments
    try
        set initialFolder to POSIX file (item 1 of arguments) as alias
        set selectedFolder to choose folder with prompt "Choose a project folder" default location initialFolder
        return "PICK" & tab & POSIX path of selectedFolder
    on error messageText number errorNumber
        if errorNumber is -128 then return "CANCEL"
        error messageText number errorNumber
    end try
end run
