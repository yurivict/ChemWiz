#!/usr/bin/env chemwiz

//
// app-neural-net-based-energy-computation.js is a research application which purpose is to aid in research on
// computation of energy of atomic configurations.
// It allows to:
// 1. Generate and keep track of many test configurations
// 2. Generate a set of neural networks that allow to compute the enery very fast
// 3. Compute enery and dynamics of complex molecular systems
//

/// computation engine that we use

var Engine = require('calc-nwchem').create()
//var Engine = require('calc-erkale').create()
//print("Using the "+Engine.kind()+" QC engine");
var cparams = {dir:"/tmp", precision: "0.01"}

/// functions

function usage() {
	throw "Usage: app-neural-net-based-energy-computation.js {db|generate|compute} ...\n";
}

function calcEnergy(engine, params, molecule) {
	var time1 = new Date().getTime();
	var energy = engine.calcEnergy(molecule, params)
	var time2 = new Date().getTime();
	return {engine: engine.kind(), energy: energy, precision: params.precision, elapsed: time2-time1, timestamp: time2};
}

var num = 0;

function addSmilesFromFile(fileName) {
	File.read(fileName).split("\n").forEach(function(smi) {
		if (smi=="")
			return;

		num++;
		print("===> (num="+num+") smi="+smi);
		try {
			var m = Moleculex.fromSMILES(smi);
			writeXyzFile(m, "m-"+smi+".xyz");
			print("RESULT (num="+num+"): energy="+JSON.stringify(calcEnergy(Engine, cparams, m)));
		} catch (err) {
			print("EXCEPTION (num="+num+"): "+err);
		}
	});
}

var actions = {
	db: {
		// internals
		_dbFileName_: "nn-energy-computations.sqlite",
		open: function() {
			return require('sqlite3').openDatabase(this._dbFileName_);
		},
		// db ... commands
		create: function() {
			File.unlink(this._dbFileName_);
			var db = this.open();
			db.run("CREATE TABLE energy(id INTEGER PRIMARY KEY AUTOINCREMENT, energy REAL, precision REAL, elapsed INTEGER, timestamp INTEGER, engine TEXT);");
			db.run("CREATE TABLE xyz(energy_id INTEGER, elt TEXT, x REAL, y REAL, z REAL, FOREIGN KEY(energy_id) REFERENCES energy(id));");
			db.run("CREATE VIEW energy_view AS SELECT e.*"+
				", (SELECT COUNT(*) FROM xyz WHERE xyz.energy_id = e.id) AS num_atoms"+
				", (SELECT GROUP_CONCAT(xyz.elt) FROM xyz WHERE xyz.energy_id = e.id) AS formula"+
				" FROM energy e;");
			db.close();
		},
		stats: function() {
		},
		// internally-used commands
		insertEnergy: function(db, energy, precision, elapsed, timestamp, engine, molecule) {
			var db = this.open();
			db.run("BEGIN TRANSACTION")
			db.run("INSERT INTO energy(energy,precision,elapsed,timestamp,engine) VALUES ("+energy+", "+precision+", "+elapsed+", "+timestamp+", '"+engine+"');")
			db.run("SELECT MAX(id) FROM energy;", function(opaque,nCols,fldValues,fldNamesValues) {
				var energy_id = fldValues[0];
				print("energy_id="+energy_id);
				for (var a = 0; a < molecule.numAtoms(); a++) {
					var atom = molecule.getAtom(a);
					var atomPos = atom.getPos();
					db.run("INSERT INTO xyz(energy_id,elt,x,y,z) VALUES ("+energy_id+", '"+atom.getElement()+"', "+atomPos[0]+", "+atomPos[1]+", "+atomPos[2]+");")
				}
			});
			db.run("COMMIT")
		}
	},
	generate: {
		// internals
		_fromSmiles_: function(db, smi) {
			var molecule = Moleculex.fromSMILES(smi);
			var rec = calcEnergy(Engine, cparams, molecule);
			actions.db.insertEnergy(db, rec.energy, rec.precision, rec.elapsed, rec.timestamp, rec.engine, molecule);
		},
		// generate ... commands
		fromSmilesText: function(smiText) {
			var db = actions.db.open();
			this._fromSmiles_(db, smiText);
			db.close();
		},
		fromSmilesFile: function(fileName) {
			var db = actions.db.open();
			var fromSmiles = this._fromSmiles_;
			File.read(fileName).split("\n").forEach(function(smi) {
				if (smi=="")
					return;
				fromSmiles(db, smi);
			});
			db.close();
		}
	}
};


/// MAIN

function main(args) {
	if (args.length < 1)
		usage();

	if (args[0] == "db") {
		if (args.length < 2)
			usage();
		else if (args[1] == "create") {
			actions.db.create();
		} else if (args[1] == "stats")
			actions.db.stats();
		else
			usage();
	} else if (args[0] == "generate") {
		if (args.length < 2) {
			usage();
		} else if (args[1] == "from-smiles-text") {
			for (var i = 2; i < args.length; i++)
				actions.generate.fromSmilesText(args[i]);
		} else if (args[1] == "from-smiles-file") {
			for (var i = 2; i < args.length; i++)
				actions.generate.fromSmilesFile(args[i]);
		} else
			usage();
	} else if (args[0] == "compute") {
	} else {
		usage();
	}
}

main(scriptArgs);
print("--END--");

//addSmilesFromFile("smiles-list.txt");
