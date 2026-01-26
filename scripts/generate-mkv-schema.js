const fs = require("fs");
const { XMLParser } = require("fast-xml-parser");
const { EOL } = require("node:os");

const kSchemaFile = "ebml_matroska.xml";
const kSchemaJSON = kSchemaFile.replace(".xml", ".json");

const Download = async () => {
    const ebml = await fetch(
        "https://raw.githubusercontent.com/ietf-wg-cellar/ebml-specification/refs/heads/master/ebml.xml"
    );
    const ebmlData = await ebml.text();

    const matroska = await fetch(
        "https://github.com/ietf-wg-cellar/matroska-specification/raw/refs/heads/master/ebml_matroska.xml"
    );
    const matroskaData = await matroska.text();

    const combined = `<Schemas>${ebmlData}${matroskaData}</Schemas>`;
    fs.writeFileSync(kSchemaFile, combined);

    const parser = new XMLParser({
        ignoreAttributes: false,
        attributeNamePrefix: "",
    });
    const json = parser.parse(combined);
    fs.writeFileSync(kSchemaJSON, JSON.stringify(json, null, 4));
};

const Generate = async () => {
    const forceRefresh = false;
    if (
        !fs.existsSync(kSchemaFile) ||
        !fs.existsSync(kSchemaJSON) ||
        forceRefresh
    )
        await Download();

    let enumDecls = [];
    let idDecls = [];
    let idMappings = [];

    const kDataTypeMap = {
        master: "kMaster",
        uinteger: "kUnsignedInt",
        integer: "kInteger",
        float: "kFloat",
        string: "kString",
        "utf-8": "kUnicode",
        binary: "kBinary",
        date: "kDate",
    };

    const TranslateDataType = (type) => {
        if (type in kDataTypeMap)
            return `MatroskaDataType::${kDataTypeMap[type]}`;
        console.log("nae " + type);
        return `MatroskaDataType::kUnknown`;
    };

    const schema = JSON.parse(fs.readFileSync(kSchemaJSON).toString());

    const injected = [{ name: "Unknown", type: "unknown", id: "0x0000" }];
    let flattened = [];
    schema.Schemas.EBMLSchema.map((x) => flattened.push(...x.element));
    flattened = flattened.map((x) => {
        return { id: x.id, name: x.name, type: x.type };
    });

    let elements = [...injected, ...flattened];
    elements = elements.filter((value, index, array) => {
        const search = array.findIndex((v) => v.id == value.id);
        if (search != index) console.log(value.name);
        return search == index;
    });

    let longestName = "";
    elements.forEach((elem) => {
        if (elem.name.length > longestName.length) longestName = elem.name;
    });

    elements.forEach((elem) => {
        elem.name = elem.name.replaceAll("-", "");
        let pad = " ".repeat(longestName.length - elem.name.length);
        const id = `MatroskaElementId::k${elem.name}`;

        idMappings.push(`        { ${id}, k${elem.name} },`);

        enumDecls.push(`        k${elem.name}${pad} = ${elem.id},`);
        idDecls.push(
            `    static const MatroskaIdentifier k${
                elem.name
            } ${pad} = { ${id}, ${TranslateDataType(elem.type)}, "${
                elem.name
            }" };`
        );
    });

    const replaces = {
        EnumDecls: enumDecls.join(EOL),
        IDDecls: idDecls.join(EOL),
        IDMapping: idMappings.join(EOL),
    };

    const files = ["AX-MKVSchema.h.template", "AX-MKVSchema.cxx.template"];
    files.map((file) => {
        let template = fs.readFileSync(file).toString();
        for (r in replaces) {
            template = template.replaceAll(`\${${r}}`, replaces[r]);
        }

        fs.writeFileSync(`../src/AX/MKV/${file.replace(".template", "")}`, template);
    });
};

(async () => {
    await Generate();
})();
